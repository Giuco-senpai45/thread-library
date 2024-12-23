#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#define THREAD_COUNT 10

void ms_sleep(int milliseconds) {
    usleep(milliseconds * 1e3);
}

void* f(void* arg) {
    pthread_t self = pthread_self();
    for (size_t i = 0; i < 100; i++) {
        ms_sleep(100);
        printf("Hello from thread %lu (%lu)\n", (unsigned long)self, i);
    }
    
    unsigned long* result = malloc(sizeof(unsigned long));
    *result = (unsigned long)self * 11;
    return result;
}

int main() {
    pthread_t threads[THREAD_COUNT];
    unsigned long* retvals[THREAD_COUNT];
    int status;

    for (size_t i = 0; i < THREAD_COUNT; i++) {
        status = pthread_create(&threads[i], NULL, f, NULL);
        if (0 != status) {
            printf("Failed to create pthread: %s\n", strerror(status));
            exit(status);
        }
    }

    for (size_t i = 0; i < THREAD_COUNT; i++) {
        status = pthread_join(threads[i], (void**)&retvals[i]);
        if (0 != status) {
            printf("Failed to join pthread: %s\n", strerror(status));
            exit(status);
        }
        printf("Joined thread %lu with return value %lu\n", 
               (unsigned long)threads[i], *retvals[i]);
        free(retvals[i]);
    }

    return 0;
}