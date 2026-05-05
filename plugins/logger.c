
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "plugin_common.h"

__attribute__((visibility("default")))
const char* plugin_get_name(void) {
    return "logger";
}

const char* plugin_transform(const char* input) {
    if (!input) return NULL;

    fprintf(stdout, "[logger] %s\n", input);
    fflush(stdout); // Ensure immediate output
    
    size_t len = strlen(input);
    char* output = malloc(len + 1);
    if (!output) return NULL;

    strcpy(output, input);
    return output;
}

const char* plugin_init(int queue_size) {
    return common_plugin_init(plugin_transform, "logger", queue_size);
}
