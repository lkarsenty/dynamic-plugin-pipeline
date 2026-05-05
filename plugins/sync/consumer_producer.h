#ifndef CONSUMER_PRODUCER_H
#define CONSUMER_PRODUCER_H

#include <stddef.h>
#include <pthread.h>
#include "monitor.h"

/**
 * Consumer-Producer queue structure for thread-safe producer-consumer pattern
 * Now using monitors for simpler implementation
 */
typedef struct {
    char      **items;              /* Array of string pointers (ownership: queue) */
    int         capacity;           /* Maximum number of items */
    int         count;              /* Current number of items */
    int         head;               /* Index of first item  */
    int         tail;               /* Index of next insertion point (next put) */
    int         finished;           /* Finished processing flag */

    pthread_mutex_t lock;           /* Mutex for protecting access to queue state */

    monitor_t   not_full_monitor;   /* state=1 when count < capacity, else 0 */
    monitor_t   not_empty_monitor;  /* state=1 when count > 0, else 0 */
    monitor_t   finished_monitor;   /* state=1 when queue is finished/closed */
} consumer_producer_t;

/**
 * Initialize a consumer-producer queue
 * @param queue    Pointer to queue structure
 * @param capacity Maximum number of items (>0)
 * @return NULL on success, error message on failure
 */
const char* consumer_producer_init(consumer_producer_t* queue, int capacity);

/**
 * Destroy a consumer-producer queue and free its resources
 * @param queue Pointer to queue structure
 */
void consumer_producer_destroy(consumer_producer_t* queue);

/**
 * Add an item to the queue (producer). 
 * Blocks if queue is full.
 * @param queue Pointer to queue structure
 * @param item  String to add (queue takes ownership)
 * @return NULL on success, error message on failure
 */
const char* consumer_producer_put(consumer_producer_t* queue, const char* item);

/**
 * Remove an item from the queue (consumer) and return it.
 * Blocks if queue is empty.
 * @param queue Pointer to queue structure
 * @return String item or NULL if finished AND empty
 */
char* consumer_producer_get(consumer_producer_t* queue);

/**
 * Signal that processing is finished.
 * @param queue Pointer to queue structure
 */
void consumer_producer_signal_finished(consumer_producer_t* queue);

/**
 * Wait for processing to be finished.
 * @param queue Pointer to queue structure
 * @return 0 on success, -1 on timeout.
 */
int consumer_producer_wait_finished(consumer_producer_t* queue);

#endif // CONSUMER_PRODUCER_H
