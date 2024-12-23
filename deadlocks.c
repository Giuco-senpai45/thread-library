#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "lib/mutex.h"
#include "lib/ult.h"
#include "lib/utils.h"

#define MAX_SLEEP 5

tid_t m1;
tid_t m2;
tid_t m3;

void* f1(void* arg) {
  printf("[f1] Attempting to acquire mutex1\n");
  ult_mutex_lock(m1);
  printf("[f1] Acquired mutex1, now attempting mutex2\n");

  // Simulate work
  ms_sleep(500);

  ult_mutex_lock(m2);
  printf("[f1] Acquired mutex2\n");

  ult_mutex_unlock(m2);
  ult_mutex_unlock(m1);

  return NULL;
}

void* f2(void* arg) {
  printf("[f2] Attempting to acquire mutex2\n");
  ult_mutex_lock(m2);
  printf("[f2] Acquired mutex2, now attempting mutex1\n");

  // Simulate work
  ms_sleep(500);

  ult_mutex_lock(m1);
  printf("[f2] Acquired mutex1\n");

  ult_mutex_unlock(m1);
  ult_mutex_unlock(m2);

  return NULL;
}

void* f3(void* arg) {
    printf("[f3] Attempting to acquire mutex3\n");
    ult_mutex_lock(m3);
    printf("[f3] Acquired mutex3\n");

    // Simulate some independent work
    for(int i = 0; i < 10; i++) {
        printf("[f3] Working... step %d\n", i + 1);
        ms_sleep(300);
    }

    printf("[f3] Releasing mutex3\n");
    ult_mutex_unlock(m3);

    return NULL;
}

int main() {
    tid_t t1, t2, t3;
    if (ult_init(100000) != 0) {
        perror("Failed to initialize ULT library");
        return EXIT_FAILURE;
    }

    if (ult_mutex_init(&m1) != 0 || ult_mutex_init(&m2) != 0 || ult_mutex_init(&m3) != 0) {
        perror("Failed to initialize mutexes");
        return EXIT_FAILURE;
    }

    if (ult_create(&t1, &f1, NULL) != 0 ||
        ult_create(&t2, &f2, NULL) != 0 ||
        ult_create(&t3, &f3, NULL) != 0) {
        perror("Failed to create threads");
        return EXIT_FAILURE;
    }

    if (ult_join(t1, NULL) != 0 ||
        ult_join(t2, NULL) != 0 ||
        ult_join(t3, NULL) != 0) {
        perror("Failed to join threads");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
