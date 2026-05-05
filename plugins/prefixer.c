#include <stdlib.h>
#include <string.h>

#include "plugin_common.h"

#define PREFIX "[prefix] "

__attribute__((visibility("default")))
const char* plugin_get_name(void) {
    return "prefixer";
}

static const char* plugin_transform(const char* input) {
    if (!input) return NULL;

    size_t prefix_len = strlen(PREFIX);
    size_t input_len = strlen(input);
    char* output = malloc(prefix_len + input_len + 1);
    if (!output) return NULL;

    memcpy(output, PREFIX, prefix_len);
    memcpy(output + prefix_len, input, input_len + 1);
    return output;
}

__attribute__((visibility("default")))
const char* plugin_init(int queue_size) {
    return common_plugin_init(plugin_transform, "prefixer", queue_size);
}
