#ifndef _BATCHER_H_
#define _BATCHER_H_

#include <pthread.h>
#include <stdatomic.h>


typedef struct {
    pthread_cond_t waiters;
    atomic_int counter;
    atomic_int remaining;
} batcher_t;


int get_batcher_epoch(batcher_t *b);

void enter_batcher(batcher_t *b);

void leave_batcher(batcher_t *b);


#endif
