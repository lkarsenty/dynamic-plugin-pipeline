#include <stdlib.h>
#include <string.h>
#include "plugin_common.h"

__attribute__((visibility("default")))
const char* plugin_get_name(void) {
    return "expander";
}

static const char* plugin_transform(const char* input) {
    if (!input) return NULL;

    size_t len = strlen(input);
    if (len == 0) {
        return strdup("");
    }

    size_t new_len = len * 2 - 1;
    char* output = malloc(new_len + 1);
    if (!output) return NULL;

    for (size_t i = 0; i < len; i++) {

        output[i * 2] = input[i];
        if (i < len - 1) {
            output[i * 2 + 1] = ' ';
        }
    }
    output[new_len] = '\0';
    return output;
}

__attribute__((visibility("default")))
const char* plugin_init(int queue_size) {
    return common_plugin_init(plugin_transform, "expander", queue_size);
}