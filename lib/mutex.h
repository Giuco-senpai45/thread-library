#ifndef ULT_MUTEX_H
#define ULT_MUTEX_H
#include "ult.h"

typedef struct ult_mutex {
    tid_t id;                                  // Mutex identifier
    tid_t holder;                              // Current thread holding the mutex
    size_t waiting_count;                      // Number of threads waiting
    bool waiting_threads[MAX_THREADS_COUNT];    // Array tracking which threads are waiting
} ult_mutex_t;

int ult_mutex_init(tid_t* mutex_id);
int ult_mutex_lock(tid_t mutex_id);
int ult_mutex_unlock(tid_t mutex_id);
int ult_mutex_destroy(tid_t mutex_id);
void display_deadlocks();
#endif