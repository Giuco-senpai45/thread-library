#include <stdio.h>
#include <stdlib.h>
#include "lib/ult.h"
#include "lib/mutex.h"
#include "lib/cond.h"
#include "lib/utils.h"

#define BUFFER_SIZE 1
#define NUM_ITEMS 5  // Reduced for clarity
#define NUM_PRODUCERS 2  // Reduced for clarity
#define NUM_CONSUMERS 2  // Reduced for clarity

typedef struct {
    int buffer[BUFFER_SIZE];
    int in;
    int out;
    int count;
    tid_t mutex;
    cid_t not_full;
    cid_t not_empty;
} shared_buffer_t;

shared_buffer_t shared_buffer;

void* producer(void* arg) {
    int producer_id = *(int*)arg;
    int items_produced = 0;

    while (items_produced < NUM_ITEMS) {
        ms_sleep(1000);
        int item = producer_id * 100 + items_produced;

        printf("\nProducer %d: Attempting to produce item %d\n", producer_id, item);
        ult_mutex_lock(shared_buffer.mutex);

        while (shared_buffer.count == BUFFER_SIZE) {
            printf("Producer %d: Buffer full, waiting...\n", producer_id);
            ult_cond_wait(shared_buffer.not_full, shared_buffer.mutex);
            printf("Producer %d: Woke up from wait\n", producer_id);
        }

        shared_buffer.buffer[shared_buffer.in] = item;
        shared_buffer.in = (shared_buffer.in + 1) % BUFFER_SIZE;
        shared_buffer.count++;

        printf("Producer %d: Produced item %d (buffer count: %d)\n",
               producer_id, item, shared_buffer.count);

        printf("Producer %d: Signaling not_empty\n", producer_id);
        ult_cond_signal(shared_buffer.not_empty);
        ult_mutex_unlock(shared_buffer.mutex);

        items_produced++;
    }

    printf("Producer %d: Finished producing\n", producer_id);
    return NULL;
}

void* consumer(void* arg) {
    int consumer_id = *(int*)arg;
    int items_consumed = 0;

    while (items_consumed < NUM_ITEMS) {
        ms_sleep(2000);

        printf("\nConsumer %d: Attempting to consume\n", consumer_id);
        ult_mutex_lock(shared_buffer.mutex);

        while (shared_buffer.count == 0) {
            printf("Consumer %d: Buffer empty, waiting...\n", consumer_id);
            ult_cond_wait(shared_buffer.not_empty, shared_buffer.mutex);
            printf("Consumer %d: Woke up from wait\n", consumer_id);
        }

        int item = shared_buffer.buffer[shared_buffer.out];
        shared_buffer.out = (shared_buffer.out + 1) % BUFFER_SIZE;
        shared_buffer.count--;

        printf("Consumer %d: Consumed item %d (buffer count: %d)\n",
               consumer_id, item, shared_buffer.count);

        printf("Consumer %d: Signaling not_full\n", consumer_id);
        ult_cond_signal(shared_buffer.not_full);
        ult_mutex_unlock(shared_buffer.mutex);

        items_consumed++;
    }

    printf("Consumer %d: Finished consuming\n", consumer_id);
    return NULL;
}

int main() {
    printf("Starting Producer-Consumer Program\n\n");

    if (ult_init(50000) != EXIT_SUCCESS) {
        fprintf(stderr, "Failed to initialize ULT system\n");
        return EXIT_FAILURE;
    }

    shared_buffer.in = 0;
    shared_buffer.out = 0;
    shared_buffer.count = 0;

    printf("Initializing synchronization primitives...\n");

    if (ult_mutex_init(&shared_buffer.mutex) != EXIT_SUCCESS) {
        fprintf(stderr, "Failed to initialize mutex\n");
        return EXIT_FAILURE;
    }

    if (ult_cond_init(&shared_buffer.not_full) != EXIT_SUCCESS ||
        ult_cond_init(&shared_buffer.not_empty) != EXIT_SUCCESS) {
        fprintf(stderr, "Failed to initialize condition variables\n");
        return EXIT_FAILURE;
    }

    printf("Creating threads...\n");

    tid_t producer_threads[NUM_PRODUCERS];
    tid_t consumer_threads[NUM_CONSUMERS];
    int producer_ids[NUM_PRODUCERS];
    int consumer_ids[NUM_CONSUMERS];

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producer_ids[i] = i;
        if (ult_create(&producer_threads[i], producer, &producer_ids[i]) != EXIT_SUCCESS) {
            fprintf(stderr, "Failed to create producer thread %d\n", i);
            return EXIT_FAILURE;
        }
        printf("Created producer %d\n", i);
    }

    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumer_ids[i] = i;
        if (ult_create(&consumer_threads[i], consumer, &consumer_ids[i]) != EXIT_SUCCESS) {
            fprintf(stderr, "Failed to create consumer thread %d\n", i);
            return EXIT_FAILURE;
        }
        printf("Created consumer %d\n", i);
    }

    printf("\nWaiting for threads to complete...\n");

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        ult_join(producer_threads[i], NULL);
    }
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        ult_join(consumer_threads[i], NULL);
    }

    ult_mutex_destroy(shared_buffer.mutex);
    ult_cond_destroy(shared_buffer.not_full);
    ult_cond_destroy(shared_buffer.not_empty);

    printf("\nAll threads completed successfully\n");
    return EXIT_SUCCESS;
}
