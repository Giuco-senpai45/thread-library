#ifndef ULT_H
#define ULT_H

#include <stdbool.h>
#include <stdlib.h>
#include <ucontext.h>

#define MAX_THREADS_COUNT 1000

typedef enum state_t { ULT_BLOCKED, ULT_READY, ULT_TERMINATED } state_t;

typedef unsigned long int tid_t;

typedef struct ult {
    tid_t tid;
    state_t state;
    ucontext_t context;
    void *(*start_routine)(void *);
    void *arg;
    void *retval;

    tid_t waiting_for;
    bool has_joiner;
    tid_t joiner;
} ult_t;

int ult_init(long quantum);
int ult_create(tid_t *thread_id, void *(*start_routine)(void *), void *arg);
int ult_join(tid_t thread_id, void **retval);
tid_t ult_self();
void ult_exit(void *retval);
void ult_yield(void);

bool ult_is_thread_waiting_for(tid_t waiter, tid_t target);
bool ult_is_thread_terminated(tid_t tid);
ult_t* get_current_thread();
ult_t* get_thread_by_id(tid_t tid);
size_t ult_get_thread_count();

#endif
