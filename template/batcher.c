#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

#include "batcher.h"

int get_batcher_epoch(batcher_t *b) { return b->counter; }

void init_batcher(batcher_t *b) {
  // TODO: add unlikely here, remove asserts?
  assert(pthread_mutex_init(&b->critsec, NULL) == 0);
  assert(pthread_cond_init(&b->waiters, NULL) == 0);
}

void enter_batcher(batcher_t *b) {
  // TODO: rethink critsec for optimization purposes
  assert(pthread_mutex_lock(&b->critsec) == 0);

  if (!__sync_bool_compare_and_swap(&b->remaining, 0, 1)) {
    b->blocked++;
    pthread_cond_wait(&b->waiters, &b->critsec);
  }

  assert(pthread_mutex_unlock(&b->critsec) == 0);
}

void leave_batcher(batcher_t *b) {
  assert(pthread_mutex_lock(&b->critsec) == 0);

  b->remaining--;

  if (__sync_bool_compare_and_swap(&b->remaining, 0, b->blocked)) {
    b->counter++;
    pthread_cond_broadcast(&b->waiters);
  }

  assert(pthread_mutex_unlock(&b->critsec) == 0);
}
