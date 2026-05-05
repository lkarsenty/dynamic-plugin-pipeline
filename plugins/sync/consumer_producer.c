#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "monitor.h"
#include "consumer_producer.h"
#include "../plugin_common.h"

#define DEBUG 0


const char* consumer_producer_init(consumer_producer_t* queue, int capacity) {
    if (!queue) {
        return "queue pointer is NULL";
    }

    if (capacity <= 0) {
        return "Capacity must be greater than 0";
    }

    queue->items = malloc((size_t)capacity * sizeof(char*));
    if (!queue->items) {
        return "Failed to allocate memory for items";
    }

    // Initialize all pointers to NULL
    for (int i = 0; i < capacity; i++) {
        queue->items[i] = NULL;
    }

    queue->capacity = capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->finished = 0;

    if (pthread_mutex_init(&queue->lock, NULL) != 0) {
        free(queue->items);
        queue->items = NULL;
        return "mutex_init failed";
    }

    if (monitor_init(&queue->not_full_monitor) != 0) {
        pthread_mutex_destroy(&queue->lock);
        free(queue->items);
        queue->items = NULL;
        return "monitor_init(not_full) failed";
    }
    
    if (monitor_init(&queue->not_empty_monitor) != 0) {
        monitor_destroy(&queue->not_full_monitor);
        pthread_mutex_destroy(&queue->lock);
        free(queue->items);
        queue->items = NULL;
        return "monitor_init(not_empty) failed";
    }
    
    if (monitor_init(&queue->finished_monitor) != 0) {
        monitor_destroy(&queue->not_empty_monitor);
        monitor_destroy(&queue->not_full_monitor);
        pthread_mutex_destroy(&queue->lock);
        free(queue->items);
        queue->items = NULL;
        return "monitor_init(finished) failed";
    }
    return NULL;
}

void consumer_producer_destroy(consumer_producer_t* queue) {
    if (!queue) return;

    // Lock to ensure thread safety during cleanup
    pthread_mutex_lock(&queue->lock);
    
    if (queue->items) {
        // Free any remaining strings in the queue
        for (int i = 0; i < queue->count; i++) {
            int idx = (queue->head + i) % queue->capacity;
            if (queue->items[idx]) {
                free(queue->items[idx]);
                queue->items[idx] = NULL;
            }
        }
        free(queue->items);
        queue->items = NULL;
    }

    pthread_mutex_unlock(&queue->lock);

    monitor_destroy(&queue->not_full_monitor);
    monitor_destroy(&queue->not_empty_monitor);
    monitor_destroy(&queue->finished_monitor);
    pthread_mutex_destroy(&queue->lock);

    queue->capacity = 0;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->finished = 0;
}

const char* consumer_producer_put(consumer_producer_t* queue, const char* item) {
    if (!queue || !item) return "queue pointer or item is NULL";

    while (1) {
        pthread_mutex_lock(&queue->lock);

        if (queue->finished) {
            pthread_mutex_unlock(&queue->lock);
            #if DEBUG
            log_error(NULL, "Tried to put item after queue finished");
            #endif
            return "queue is finished";
        }

        // Check if there's space in the queue
        if (queue->count < queue->capacity) {
            char* copy = strdup(item);
            if (!copy) {
                pthread_mutex_unlock(&queue->lock);
                #if DEBUG
                log_error(NULL, "Failed to allocate copy of item");
                #endif
                return "malloc failed";
            }

            queue->items[queue->tail] = copy;
            queue->tail = (queue->tail + 1) % queue->capacity;
            queue->count++;

            #if DEBUG
            log_info(NULL, "[PUT] inserted item into queue");
            #endif

            monitor_signal(&queue->not_empty_monitor);
            pthread_mutex_unlock(&queue->lock);
            return NULL;
        }

        // Wait for space in the queue
        pthread_mutex_unlock(&queue->lock);
        if (monitor_wait(&queue->not_full_monitor) != 0) {
            #if DEBUG
            log_error(NULL, "Failed waiting on not_full_monitor");
            #endif
            return "Failed to wait on not_full_monitor";
        }
    }
}


char* consumer_producer_get(consumer_producer_t* queue) {
    if (!queue) return NULL;

    while (1) {
        pthread_mutex_lock(&queue->lock);

        if (queue->count > 0) {
            char* item = queue->items[queue->head];
            queue->items[queue->head] = NULL;
            queue->head = (queue->head + 1) % queue->capacity;
            queue->count--;

            #if DEBUG
            log_info(NULL, "[GET] got item from queue");
            #endif

            monitor_signal(&queue->not_full_monitor);
            pthread_mutex_unlock(&queue->lock);
            return item;
        }

        if (queue->finished) {
            pthread_mutex_unlock(&queue->lock);
            #if DEBUG
            log_info(NULL, "[GET] queue finished, returning NULL");
            #endif
            return NULL;
        }

        pthread_mutex_unlock(&queue->lock);
        if (monitor_wait(&queue->not_empty_monitor) != 0) {
            #if DEBUG
            log_error(NULL, "Failed waiting on not_empty_monitor");
            #endif
            return NULL;
        }
    }
}


void consumer_producer_signal_finished(consumer_producer_t* queue) {
    if (!queue) return;

    pthread_mutex_lock(&queue->lock);
    if (!queue->finished) {
        queue->finished = 1;
        monitor_signal(&queue->not_full_monitor);
        monitor_signal(&queue->not_empty_monitor);
        monitor_signal(&queue->finished_monitor);
    }
    pthread_mutex_unlock(&queue->lock);
}

int consumer_producer_wait_finished(consumer_producer_t* queue) {
    if (!queue) return -1;
    return monitor_wait(&queue->finished_monitor);
}

