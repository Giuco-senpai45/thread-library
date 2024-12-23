#include <stdio.h>
#include <stdlib.h>
#include "lib/ult.h"
#include "lib/cond.h"
#include "lib/mutex.h"
#include "lib/utils.h"

#define NUM_WORKERS 4

tid_t mutex_id;
cid_t cond_id;
volatile bool start_work = false;

void* worker(void* arg) {
    long worker_id = (long)arg;
    printf("Worker %ld: Initialized and waiting for signal\n", worker_id);

    ult_mutex_lock(mutex_id);
    while (!start_work) {
        printf("Worker %ld: Going to wait on condition\n", worker_id);
        ult_cond_wait(cond_id, mutex_id);
    }
    ult_mutex_unlock(mutex_id);

    printf("Worker %ld: Starting work!\n", worker_id);
    for(int i = 0; i < 3; i++) {
        printf("Worker %ld: Processing step %d\n", worker_id, i);
        ult_yield();
    }
    printf("Worker %ld: Completed work\n", worker_id);

    return NULL;
}

void* coordinator(void* arg) {
    printf("Coordinator: Initializing workers...\n");
    ms_sleep(1000);

    printf("\nCoordinator: Broadcasting start signal to all workers\n");
    ult_mutex_lock(mutex_id);
    start_work = true;
    ult_cond_broadcast(cond_id);
    ult_mutex_unlock(mutex_id);

    printf("Coordinator: Signal sent, workers should begin processing\n");
    return NULL;
}

int main() {
    if (ult_init(100000) != EXIT_SUCCESS) {
        printf("Failed to initialize ULT library\n");
        return EXIT_FAILURE;
    }

    if (ult_mutex_init(&mutex_id) != EXIT_SUCCESS) {
        printf("Failed to initialize mutex\n");
        return EXIT_FAILURE;
    }

    if (ult_cond_init(&cond_id) != EXIT_SUCCESS) {
        printf("Failed to initialize condition variable\n");
        return EXIT_FAILURE;
    }

    tid_t worker_threads[NUM_WORKERS];
    for (long i = 0; i < NUM_WORKERS; i++) {
        if (ult_create(&worker_threads[i], worker, (void*)i) != EXIT_SUCCESS) {
            printf("Failed to create worker thread %ld\n", i);
            return EXIT_FAILURE;
        }
    }

    tid_t coordinator_thread;
    if (ult_create(&coordinator_thread, coordinator, NULL) != EXIT_SUCCESS) {
        printf("Failed to create coordinator thread\n");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < NUM_WORKERS; i++) {
        ult_join(worker_threads[i], NULL);
    }
    ult_join(coordinator_thread, NULL);

    printf("\nMain: All workers have completed\n");
    return EXIT_SUCCESS;
}
