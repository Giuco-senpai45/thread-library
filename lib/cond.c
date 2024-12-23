#include "cond.h"
#include "mutex.h"
#include "utils.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>

static ult_cond_t conditions[MAX_THREADS_COUNT];
static size_t cond_count = 0;

int ult_cond_init(cid_t *cid) {
    if (MAX_THREADS_COUNT - 1 == cond_count) {
        return EXIT_FAILURE;
    }

    ult_cond_t *cv = &conditions[cond_count];
    cv->id = cond_count;
    cv->waiting_count = 0;
    memset(cv->waiting_threads, 0, sizeof(bool) * MAX_THREADS_COUNT);

    *cid = cond_count;
    cond_count++;

    return EXIT_SUCCESS;
}

int ult_cond_destroy(cid_t cid) {
    if (cid >= cond_count) {
        errno = EINVAL;
        return EXIT_FAILURE;
    }

    ult_cond_t *cv = &conditions[cid];
    if (cv->waiting_count > 0) {
        errno = EBUSY;
        return EXIT_FAILURE;
    }
    cv->id = -1;
    return EXIT_SUCCESS;
}

int ult_cond_wait(cid_t cid, tid_t mid) {
    block_signals();

    if (cid >= cond_count) {
        unblock_signals();
        errno = EINVAL;
        return EXIT_FAILURE;
    }

    ult_cond_t *cv = &conditions[cid];
    tid_t self = ult_self();
    ult_t *current = get_current_thread();

    if (!current) {
        unblock_signals();
        errno = EINVAL;
        return EXIT_FAILURE;
    }

    cv->waiting_count++;
    cv->waiting_threads[self] = true;

    ult_mutex_unlock(mid);
    current->state = ULT_BLOCKED;

    // yield control until signaled
    unblock_signals();
    ult_yield();

    // reacquire mutex after being signaled
    block_signals();
    ult_mutex_lock(mid);

    unblock_signals();
    return EXIT_SUCCESS;
}

int ult_cond_signal(cid_t cid) {
    block_signals();
    if (cid >= cond_count) {
        unblock_signals();
        errno = EINVAL;
        return EXIT_FAILURE;
    }

    ult_cond_t *cv = &conditions[cid];

    // wake up one waiting thread if any
    for (size_t i = 0; i < MAX_THREADS_COUNT; i++) {
        if (cv->waiting_threads[i]) {
            cv->waiting_threads[i] = false;
            cv->waiting_count--;

            ult_t *thread = get_thread_by_id(i);
            if (thread != NULL) {
                printf("Sending single signal from %ld to %ld\n", cid, thread->tid);

                thread->state = ULT_READY;
            }
            break;
        }
    }

    unblock_signals();
    return EXIT_SUCCESS;
}

int ult_cond_broadcast(cid_t cid) {
    block_signals();

    if (cid >= cond_count) {
        unblock_signals();
        errno = EINVAL;
        return EXIT_FAILURE;
    }

    ult_cond_t *cv = &conditions[cid];

    for (size_t i = 0; i < MAX_THREADS_COUNT; i++) {
        if (cv->waiting_threads[i]) {
            cv->waiting_threads[i] = false;
            cv->waiting_count--;

            ult_t *thread = get_thread_by_id(i);
            if (thread != NULL) {
                thread->state = ULT_READY;
            }
        }
    }

    printf("Sending broadcast signal from %ld\n", cid);
    unblock_signals();
    return EXIT_SUCCESS;
}
