#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/types.h>

#include "plugins/plugin_sdk.h"

typedef struct {
    plugin_init_func_t init;
    plugin_fini_func_t fini;
    plugin_place_work_func_t place_work;
    plugin_attach_func_t attach;
    plugin_wait_finished_func_t wait_finished;
    char* name;
    void* handle;
} plugin_handle_t;

void print_usage(void);
char* build_plugin_filename(const char* plugin_name);

int main(int argc, char* argv[]) {
    // Check arguments
    if (argc < 3) {
        fprintf(stderr, "Error: Missing arguments\n");
        print_usage();
        return 1;
    }

    for (const char *p = argv[1]; *p; p++) {
        if (*p < '0' || *p > '9') {
            fprintf(stderr, "Error: queue size must be a positive integer\n");
            print_usage();
            return 1;
        }
    }

    int queue_size = atoi(argv[1]);
    if (queue_size <= 0) {
        fprintf(stderr, "Error: queue size must be a positive integer\n");
        print_usage();
        return 1;
    }

    int num_plugins = argc - 2;

    // allocate plugins
    plugin_handle_t* plugins = calloc(num_plugins, sizeof(plugin_handle_t));
    if (!plugins) {
        perror("calloc failed");
        exit(1);
    }

    // load plugins dynamically
    for (int i = 0; i < num_plugins; i++) {
        const char* plugin_name = argv[i + 2];
        char* so_path = build_plugin_filename(plugin_name);
        if (!so_path) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(1);
        }

        void* handle = dlmopen(LM_ID_NEWLM, so_path, RTLD_NOW | RTLD_LOCAL);
        free(so_path);

        if (!handle) {
            fprintf(stderr, "Error: failed to load plugin '%s': %s\n",
                    plugin_name, dlerror());

            // cleanup loaded so far
            for (int j = 0; j < i; j++) {
                if (plugins[j].fini) plugins[j].fini();
                if (plugins[j].handle) dlclose(plugins[j].handle);
                free(plugins[j].name);
            }
            free(plugins);
            return 1;
        }
   
        plugin_handle_t* p = &plugins[i];
        p->handle = handle;
        p->name = strdup(plugin_name);

        p->init          = (plugin_init_func_t) dlsym(handle, "plugin_init");
        p->fini          = (plugin_fini_func_t) dlsym(handle, "plugin_fini");
        p->place_work    = (plugin_place_work_func_t) dlsym(handle, "plugin_place_work");
        p->attach        = (plugin_attach_func_t) dlsym(handle, "plugin_attach");
        p->wait_finished = (plugin_wait_finished_func_t) dlsym(handle, "plugin_wait_finished");

        if (!p->init || !p->fini || !p->place_work || !p->attach || !p->wait_finished) {
            fprintf(stderr, "Error: plugin '%s' missing required SDK functions\n", p->name);
            dlclose(handle);
            free(p->name);

            for (int j = 0; j < i; j++) {
                if (plugins[j].fini) plugins[j].fini();
                if (plugins[j].handle) dlclose(plugins[j].handle);
                free(plugins[j].name);
            }
            free(plugins);
            return 1;
        }
    }

    // initialize plugins
    for (int i = 0; i < num_plugins; i++) {
        const char* err = plugins[i].init(queue_size);
        if (err != NULL) {
            fprintf(stderr, "Plugin %s init failed: %s\n", plugins[i].name, err);
            for (int j = 0; j <= i; j++) {
                if (plugins[j].fini) plugins[j].fini();
                if (plugins[j].handle) dlclose(plugins[j].handle);
                free(plugins[j].name);
            }
            free(plugins);
            return 2;
        }
    }

    // attach chain
    for (int i = 0; i < num_plugins - 1; i++) {
        plugins[i].attach(plugins[i + 1].place_work);
    }

    // input loop
    char buffer[1025];
    while (fgets(buffer, sizeof(buffer), stdin)) {
        buffer[strcspn(buffer, "\n")] = 0;
        const char* err = plugins[0].place_work(buffer);
        if (err) {
            fprintf(stderr, "Error placing work in plugin %s: %s\n", plugins[0].name, err);
        }
        if (strcmp(buffer, "<END>") == 0) {
            break;
        }
    }

    // wait for completion
    for (int i = 0; i < num_plugins; i++) {
        const char* err = plugins[i].wait_finished();
        if (err) {
            fprintf(stderr, "Error waiting for plugin %s: %s\n", plugins[i].name, err);
        }
    }

    // cleanup
    for (int i = 0; i < num_plugins; i++) {
        if (plugins[i].fini) plugins[i].fini();
        if (plugins[i].handle) dlclose(plugins[i].handle);
        free(plugins[i].name);
    }
    free(plugins);

    fprintf(stdout, "Pipeline shutdown complete\n");
    return 0;
}

void print_usage(void) {
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: ./analyzer <queue_size> <plugin1> <plugin2> ... <pluginN>\n\n");
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "  queue_size    Maximum number of items in each plugin's queue\n");
    fprintf(stderr, "  plugin1..N    Names of plugins to load (without .so extension)\n\n");

    fprintf(stderr, "Available plugins:\n");
    fprintf(stderr, "  logger     - Logs all strings that pass through\n");
    fprintf(stderr, "  typewriter - Simulates typewriter effect with delays\n");
    fprintf(stderr, "  uppercaser - Converts strings to uppercase\n");
    fprintf(stderr, "  rotator    - Move every character to the right. Last character moves to the beginning.\n");
    fprintf(stderr, "  flipper    - Reverses the order of characters\n");
    fprintf(stderr, "  expander   - Expands each character with spaces\n\n");

    fprintf(stderr, "Example:\n");
    fprintf(stderr, "  ./analyzer 20 uppercaser rotator logger\n");
    fprintf(stderr, "  echo 'hello' | ./analyzer 20 uppercaser rotator logger\n");
    fprintf(stderr, "  echo '<END>' | ./analyzer 20 uppercaser rotator logger\n");
}

char* build_plugin_filename(const char* plugin_name) {
    const char* outdir = "output";
    const char* suffix = ".so";
    size_t len = strlen(outdir) + 1 + strlen(plugin_name) + strlen(suffix) + 1;

    char* filename = malloc(len);
    if (!filename) {
        perror("malloc failed in build_plugin_filename");
        return NULL;
    }
    snprintf(filename, len, "%s/%s%s", outdir, plugin_name, suffix);
    return filename;
}







