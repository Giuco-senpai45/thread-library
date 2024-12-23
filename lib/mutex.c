#include "mutex.h"
#include "utils.h"

#include "errno.h"
#include <stdio.h>
#include <string.h>
#include "assert.h"

static ult_mutex_t mutexes[MAX_THREADS_COUNT];
static size_t mutex_count = 0;

int ult_mutex_init(tid_t *mid)
{
    if (MAX_THREADS_COUNT - 1 == mutex_count)
    {
        return EXIT_FAILURE;
    }

    ult_mutex_t *m = &mutexes[mutex_count];
    m->id = mutex_count;
    m->holder = -1;
    m->waiting_count = 0;
    memset(m->waiting_threads, 0, sizeof(bool) * MAX_THREADS_COUNT); // Initialize waiting array

    *mid = mutex_count;
    mutex_count++;

    return EXIT_SUCCESS;
}

int ult_mutex_lock(tid_t mid) {
    block_signals();
    tid_t self = ult_self();

    if (mid >= mutex_count) {
        unblock_signals();
        errno = EINVAL;
        return EXIT_FAILURE;
    }

    ult_mutex_t *m = &mutexes[mid];
    ult_t *current = get_current_thread();

    if (current == NULL) {
        unblock_signals();
        errno = EINVAL;
        return EXIT_FAILURE;
    }

    printf("Thread %ld attempting to lock mutex %ld\n", self, mid);

    // we already hold the mutex
    if (m->holder == self) {
        unblock_signals();
        return EXIT_SUCCESS;
    }

    // mark ourselves as waiting for the lock on mutex
    if (!m->waiting_threads[self]) {
        m->waiting_count++;
        m->waiting_threads[self] = true;
        printf("Thread %ld waiting for mutex %ld (held by %ld)\n",
               self, mid, m->holder);
    }

    // while mutex is held by another thread
    while (m->holder != -1 && m->holder != self) {
        current->state = ULT_BLOCKED;
        unblock_signals();
        ult_yield();
        block_signals();
    }

    // we've acquired the mutex
    m->waiting_threads[self] = false;
    m->waiting_count--;
    m->holder = self;

    printf("Thread %ld acquired mutex %ld\n", self, mid);

    unblock_signals();
    return EXIT_SUCCESS;
}

int ult_mutex_unlock(tid_t mid) {
    block_signals();
    tid_t self = ult_self();
    ult_mutex_t *m = &mutexes[mid];

    // check if we actually hold this mutex
    if (m->holder != self) {
        unblock_signals();
        errno = EPERM;
        return EXIT_FAILURE;
    }

    printf("Thread %ld releasing mutex %ld\n", self, mid);

    // release the mutex
    m->holder = -1;

    // wake up one waiting thread
    for (size_t i = 0; i < MAX_THREADS_COUNT; i++) {
        if (m->waiting_threads[i]) {
            ult_t *thread = get_thread_by_id(i);
            if (thread != NULL && thread->state == ULT_BLOCKED) {
                thread->state = ULT_READY;
                break;
            }
        }
    }

    unblock_signals();
    return EXIT_SUCCESS;
}

int ult_mutex_destroy(tid_t mid)
{
    ult_mutex_t *m = &mutexes[mid];

    if (-1 != m->holder)
    {
        errno = EBUSY;
        return EXIT_FAILURE;
    }

    m->id = -1;
    return EXIT_SUCCESS;
}


void display_deadlocks(void) {
    printf("\nStarting deadlock detection...\n");

    size_t thread_count = ult_get_thread_count();
    if (thread_count == 0 || mutex_count == 0) {
        printf("No active threads or mutexes to check\n");
        return;
    }

    typedef struct {
        tid_t holder;
        tid_t waiting_thread;
        tid_t mutex_id;
    } mutex_relation_t;

    mutex_relation_t *relations = calloc(thread_count, sizeof(mutex_relation_t));
    if (!relations) {
        printf("Memory allocation failed during deadlock detection\n");
        return;
    }

    size_t relation_count = 0;

    // collect mutex relations
    for (size_t i = 0; i < mutex_count; i++) {
        ult_mutex_t *m = &mutexes[i];

        if (m->id == (tid_t)-1) {
            continue;
        }

        for (size_t t = 0; t < thread_count; t++) {
            if (m->waiting_threads[t]) {
                if (relation_count < thread_count) {
                    relations[relation_count].holder = m->holder;
                    relations[relation_count].waiting_thread = t;
                    relations[relation_count].mutex_id = m->id;
                    relation_count++;
                }
            }
        }
    }

    // check deadlocks by looking for cycles in relations
    bool deadlock_found = false;
    for (size_t i = 0; i < relation_count; i++) {
        tid_t start_thread = relations[i].waiting_thread;
        tid_t current_thread = relations[i].holder;

        bool *visited = calloc(thread_count, sizeof(bool));
        if (!visited) {
            continue;
        }

        visited[start_thread] = true;

        bool cycle_found = false;
        printf("\nChecking chain starting with Thread %ld:\n", start_thread);

        while (current_thread != -1) {
            if (visited[current_thread]) {
                cycle_found = true;
                deadlock_found = true;
                break;
            }

            visited[current_thread] = true;

            // find next relation where current thread is waiting
            bool found_next = false;
            for (size_t j = 0; j < relation_count; j++) {
                if (relations[j].waiting_thread == current_thread) {
                    current_thread = relations[j].holder;
                    found_next = true;
                    break;
                }
            }

            if (!found_next) {
                current_thread = -1;
            }
        }

        if (cycle_found) {
            printf("\nDEADLOCK DETECTED! Circular wait involving Thread %ld\n", start_thread);
        }

        free(visited);
    }

    if (!deadlock_found) {
        printf("No deadlocks detected\n");
    }

    free(relations);
}
