
#include <stdlib.h>
#include <string.h>

#include "plugin_common.h"

__attribute__((visibility("default")))
const char* plugin_get_name(void) {
    return "uppercaser";
}

static const char* plugin_transform(const char* input) {
    if (!input) return NULL;

    size_t len = strlen(input);
    char* output = malloc(len + 1);
    if (!output) return NULL;

    for (size_t i = 0; i < len; i++) {
        if (input[i] >= 'a' && input[i] <= 'z') {
            output[i] = input[i] - ('a' - 'A'); // shift to uppercase
        } else {
            output[i] = input[i];
        }
    }
    output[len] = '\0';

    return output;
}

const char* plugin_init(int queue_size) {
    return common_plugin_init(plugin_transform, "uppercaser", queue_size);
}



