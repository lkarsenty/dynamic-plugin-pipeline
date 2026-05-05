
#include <stdlib.h>
#include <string.h>

#include "plugin_common.h"

__attribute__((visibility("default")))
const char* plugin_get_name(void) {
    return "flipper";
}

static const char* plugin_transform(const char* input) {
    if (!input) return NULL;

    size_t len = strlen(input);
    char* output = malloc(len + 1);
    if (!output) return NULL;

    // Flip the string
    for (size_t i = 0; i < len; i++) {
        output[i] = input[len - 1 - i];
    }
    output[len] = '\0';

    return output;
}

const char* plugin_init(int queue_size) {
    return common_plugin_init(plugin_transform, "flipper", queue_size);
}