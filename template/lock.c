#define _GNU_SOURCE

#include <sched.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "lock.h"

bool spinlock_acquire(spinlock_t *lock) {
  bool expected = false;
  while (unlikely(!atomic_compare_exchange_weak_explicit(
      &(lock->locked), &expected, true, memory_order_acquire,
      memory_order_relaxed))) {
    expected = false;
    while (
        unlikely(atomic_load_explicit(&(lock->locked), memory_order_relaxed)))
      sched_yield();
  }
  return true;
}

/* Reader-writer shared lock */

bool rwlock_init(rwlock_t *lock) {
  return (pthread_rwlock_init(lock, NULL) == 0);
}

void rwlock_cleanup(rwlock_t *lock) { pthread_rwlock_destroy(lock); }

bool rwlock_acquire_writer(rwlock_t *lock) {
  return (pthread_rwlock_wrlock(lock) == 0);
}

bool rwlock_acquire_reader(rwlock_t *lock) {
  return (pthread_rwlock_rdlock(lock) == 0);
}

void rwlock_release(rwlock_t *lock) { pthread_rwlock_unlock(lock); }
