#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

#include "batcher.h"
#include "common.h"

int get_batcher_epoch(batcher_t *b) { return b->counter; }

void init_batcher(batcher_t *b) {
  // TODO: add unlikely here, remove asserts?
  assert(pthread_mutex_init(&b->critsec, NULL) == 0);
  assert(pthread_cond_init(&b->waiters, NULL) == 0);
}

void enter_batcher(batcher_t *b) {
  // TODO: rethink critsec for optimization purposes
  assert(pthread_mutex_lock(&b->critsec) == 0);

  if (!CAS(&b->remaining, 0, 1)) {
    atomic_fetch_add(&b->blocked, 1);
    pthread_cond_wait(&b->waiters, &b->critsec);
  }

  assert(pthread_mutex_unlock(&b->critsec) == 0);
}

void leave_batcher(batcher_t *b) {
  assert(pthread_mutex_lock(&b->critsec) == 0);

  atomic_fetch_add(&b->remaining, -1);

  if (CAS(&b->remaining, 0, b->blocked)) {
    atomic_fetch_add(&b->counter, 1);
    pthread_cond_broadcast(&b->waiters);
  }

  assert(pthread_mutex_unlock(&b->critsec) == 0);
}
