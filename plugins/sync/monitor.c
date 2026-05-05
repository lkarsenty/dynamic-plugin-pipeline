#include "monitor.h"
#include <errno.h> 

int monitor_init(monitor_t* monitor) {
    // Validate pointer
    if (!monitor) {
        errno = EINVAL;         
        return -1;
    }

    monitor->state = 0;

    int err = pthread_mutex_init(&monitor->mutex, NULL);
    if (err != 0) {
        errno = err; 
        return -1;
    }

    err = pthread_cond_init(&monitor->cond, NULL);
    if (err != 0) {
        pthread_mutex_destroy(&monitor->mutex); // cleanup
        errno = err;
        return -1;
    }

    return 0;
}

void monitor_destroy(monitor_t* monitor) {
    if (!monitor) {
        return;
    }

    pthread_cond_destroy(&monitor->cond);
    pthread_mutex_destroy(&monitor->mutex);
    monitor->state = 0; // Reset state
}

void monitor_signal(monitor_t* monitor) {
    if (!monitor) return;

    int err = pthread_mutex_lock(&monitor->mutex);
    if (err != 0) { errno = err; return; }

    monitor->state = 1;
    err = pthread_cond_signal(&monitor->cond);
    if (err != 0) { errno = err; }
    err = pthread_mutex_unlock(&monitor->mutex);
    if (err != 0) { errno = err; }
}

void monitor_reset(monitor_t* monitor) {
    if (!monitor) return;

    int err = pthread_mutex_lock(&monitor->mutex);
    if (err != 0) { errno = err; return; }
    monitor->state = 0;
    err = pthread_mutex_unlock(&monitor->mutex);
    if (err != 0) { errno = err; }
}

int monitor_wait(monitor_t* monitor) {
    if (!monitor) {
        errno = EINVAL; 
        return -1;
    }

    int err = pthread_mutex_lock(&monitor->mutex);
    if (err != 0) { errno = err; return -1; }

    int rc = 0; // assume success
    while (monitor->state == 0) {
        err = pthread_cond_wait(&monitor->cond, &monitor->mutex);
        if (err != 0) { errno = err; rc = -1; break; }
    }

    if (rc == 0) {
        monitor->state = 0;
    }

    int uerr = pthread_mutex_unlock(&monitor->mutex);
    if (uerr != 0) { errno = uerr; return -1; }

    return rc;
}