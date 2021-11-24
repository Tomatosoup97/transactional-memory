#ifndef _BATCHER_H_
#define _BATCHER_H_

#include <pthread.h>
#include <stdatomic.h>

#define COARSE_LOCK 1

struct region_s;

typedef struct {
  atomic_int counter;
  atomic_int remaining;
  atomic_int blocked;
  pthread_cond_t waiters;
  pthread_mutex_t critsec;
} batcher_t;

void init_batcher(batcher_t *b);

int get_batcher_epoch(batcher_t *b);

void enter_batcher(batcher_t *b);

void leave_batcher(struct region_s *region);

#endif
