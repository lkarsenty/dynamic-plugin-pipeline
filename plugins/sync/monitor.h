#ifndef MONITOR_H
#define MONITOR_H

#include <pthread.h>

typedef struct monitor {
    pthread_mutex_t mutex;   /* Mutex for thread safety */
    pthread_cond_t  cond;    /* Condition variable */
    int             state;   /* Internal flag: 0 = not signaled, 1 = signaled */
} monitor_t;

/**
 * Initialize the monitor.
 * @param monitor Pointer to monitor structure
 * @return 0 on success, -1 on failure
 */
int monitor_init(monitor_t* monitor);

/**
* Destroy a monitor and free its resources
* @param monitor Pointer to monitor structure
*/
void monitor_destroy(monitor_t* monitor);

/**
* Signal a monitor (sets the monitor state)
* @param monitor Pointer to monitor structure
*/
void monitor_signal(monitor_t* monitor);

/**
* Reset a monitor (clears the monitor state)
* @param monitor Pointer to monitor structure
*/
void monitor_reset(monitor_t* monitor);

/**
* Wait for a monitor to be signaled (infinite wait)
* @param monitor Pointer to monitor structure
* @return 0 on success, -1 on error
*/
int monitor_wait(monitor_t* monitor);

#endif // MONITOR_H