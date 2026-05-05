#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "plugin_common.h"
#include "sync/consumer_producer.h"

#define DEBUG 0

static plugin_context_t* g_ctx = NULL;

/*
static plugin_context_t g_ctx = {
    .name = NULL,
    .queue = NULL,
    .consumer_thread = 0,
    .next_place_work = NULL,
    .process_function = NULL,
    .initialized = 0,
    .finished = 0
}
*/

void* plugin_consumer_thread(void* arg) {
    plugin_context_t* ctx = (plugin_context_t*)arg;
    if (!ctx || !ctx->queue) {
        #if DEBUG
        log_error(ctx, "Invalid context or queue");
        #endif
        return NULL;
    }

    #if DEBUG
    log_info(ctx, "Thread started");
    #endif

    while (1) {
        char* in = consumer_producer_get(ctx->queue);
        if (!in) {
            #if DEBUG
            log_info(ctx, "Queue finished, exiting loop");
            #endif
            break;
        }

        if (strcmp(in, "<END>") == 0) {
            #if DEBUG
            log_info(ctx, "Received <END>");
            #endif

            if (ctx->next_place_work) {
                char* end_copy = strdup("<END>");
                if (end_copy) {
                    const char* err = ctx->next_place_work(end_copy);
                    if (err) {
                        #if DEBUG
                        log_error(ctx, "Failed to forward <END>");
                        #endif
                        free(end_copy);
                    } else {
                        #if DEBUG
                        log_info(ctx, "Forwarded <END> to next plugin");
                        #endif
                    }
                } else {
                    #if DEBUG
                    log_error(ctx, "Failed to allocate copy for <END>");
                    #endif
                }
            }

            free(in);
            break;
        }

        // Transform input
        const char* out = ctx->process_function ? ctx->process_function(in) : NULL;
        free(in);

        if (out) {
            if (ctx->next_place_work) {
                const char* err = ctx->next_place_work(out);
                if (err) {
                    #if DEBUG
                    log_error(ctx, "Failed to forward transformed string");
                    #endif
                    free((void*)out);
                } else {
                    #if DEBUG
                    log_info(ctx, "Forwarded transformed string");
                    #endif
                }
            } else {
                #if DEBUG
                log_info(ctx, "No next plugin, freeing output");
                #endif
                free((void*)out);
            }
        } else {
            #if DEBUG
            log_error(ctx, "Transform returned NULL");
            #endif
        }
    }

    ctx->finished = 1;

    consumer_producer_signal_finished(ctx->queue);
    #if DEBUG
    log_info(ctx, "Thread exiting");
    #endif
    return NULL;
}


void log_error(plugin_context_t* context, const char* message) {
    if (!message) return;
    const char* name = (context && context->name) ? context->name : "unknown";
    fprintf(stderr, "[ERROR][%s] - %s\n", name, message);
}

void log_info(plugin_context_t* context, const char* message) {
    if (!message) return;
    const char* name = (context && context->name) ? context->name : "unknown";
    fprintf(stdout, "[INFO][%s] - %s\n", name, message);
}

const char* common_plugin_init(const char* (*process_function)(const char*),
                               const char* name, int queue_size) {
    if (!process_function || !name || queue_size <= 0) {
        return "Invalid parameters to common_plugin_init";
    }

    g_ctx = malloc(sizeof(plugin_context_t));
    if (!g_ctx) {
        return "Failed to allocate context";
    }

    g_ctx->process_function = process_function;
    g_ctx->name = name;
    g_ctx->queue = malloc(sizeof(consumer_producer_t));
    if (!g_ctx->queue) {
        free(g_ctx);
        g_ctx = NULL;
        return "Failed to create queue";
    }

    const char* err = consumer_producer_init(g_ctx->queue, queue_size);
    if (err) {
        free(g_ctx->queue);
        free(g_ctx);
        g_ctx = NULL;
        return err;
    }

    if (pthread_create(&g_ctx->consumer_thread, NULL,
                       plugin_consumer_thread, g_ctx) != 0) {
        consumer_producer_destroy(g_ctx->queue);
        free(g_ctx->queue);
        free(g_ctx);
        g_ctx = NULL;
        return "Failed to create consumer thread";
    }

    g_ctx->initialized = 1;
    g_ctx->finished = 0;
    g_ctx->next_place_work = NULL;
    return NULL;
}

const char* plugin_fini(void) {
    if (!g_ctx || !g_ctx->initialized) {
        return "Plugin not initialized";
    }

    consumer_producer_signal_finished(g_ctx->queue);
    pthread_join(g_ctx->consumer_thread, NULL);

    consumer_producer_destroy(g_ctx->queue);
    free(g_ctx->queue);
    g_ctx->queue = NULL;

    free(g_ctx);
    g_ctx = NULL;

    return NULL;
}

const char* plugin_place_work(const char* str) {
    if (!g_ctx->initialized || !g_ctx->queue || !str) {
        return "Invalid plugin state or parameters";
    }

    const char* err = consumer_producer_put(g_ctx->queue, str);
    if (err != NULL) {
        return err;
    }
    return NULL;
}

void plugin_attach(const char* (*next_place_work)(const char*)) {
    g_ctx->next_place_work = next_place_work;
}

const char* plugin_wait_finished(void) {
    if (!g_ctx->initialized || !g_ctx->queue) {
        return "Plugin not initialized";
    }

    if (g_ctx->finished) {
        return NULL;
    }

    int rc = consumer_producer_wait_finished(g_ctx->queue);
    if (rc != 0) {
        return "wait_finished failed";
    }

    return NULL;
}