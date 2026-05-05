#include <stdlib.h>
#include <string.h>
#include "plugin_common.h"

__attribute__((visibility("default")))
const char* plugin_get_name(void) {
    return "rotator";
}

static const char* plugin_transform(const char* input) {
    if (!input) return NULL;

    size_t len = strlen(input);
    if (len == 0) {
        return strdup("");
    }

    char* output = malloc(len + 1);
    if (!output) return NULL;

    output[0] = input[len - 1];

    for (size_t i = 0; i < len - 1; i++) {
        output[i + 1] = input[i];
    }
    output[len] = '\0';
    return output;
}

__attribute__((visibility("default")))
const char* plugin_init(int queue_size) {
    return common_plugin_init(plugin_transform, "rotator", queue_size);
}
