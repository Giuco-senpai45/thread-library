#ifndef _COND_H
#define _COND_H

#include "ult.h"
#include <stdbool.h>

typedef size_t cid_t;

typedef struct {
    cid_t id;
    size_t waiting_count;
    bool waiting_threads[MAX_THREADS_COUNT];
} ult_cond_t;

int ult_cond_init(cid_t *cid);
int ult_cond_destroy(cid_t cid);
int ult_cond_wait(cid_t cid, tid_t mid);
int ult_cond_signal(cid_t cid);
int ult_cond_broadcast(cid_t cid);

#endif