
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> 
#include "plugin_common.h"

__attribute__((visibility("default")))
const char* plugin_get_name(void) {
    return "typewriter";
}

static const char* plugin_transform(const char* input) {
    if (!input) return NULL;

    fprintf(stdout, "[typewriter] ");
    fflush(stdout); // Ensure immediate output

    size_t len = strlen(input);
    if (len == 0) {
        return strdup("");
    }

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 100 * 1000 * 1000;

    for (size_t i = 0; i < len; i++) {
        putchar(input[i]);
        fflush(stdout);
        nanosleep(&ts, NULL);  
    }
    putchar('\n');
    fflush(stdout);

    char* copy = strdup(input);
    if (!copy) return NULL;

    return copy;
}

__attribute__((visibility("default")))
const char* plugin_init(int queue_size) {
    return common_plugin_init(plugin_transform, "typewriter", queue_size);
}
