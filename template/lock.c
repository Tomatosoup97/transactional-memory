#include <sched.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "lock.h"

bool spinlock_init(spinlock_t *lock) {
  atomic_init(&(lock->locked), false);
  return true;
}

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

void spinlock_release(spinlock_t *lock) {
  atomic_store_explicit(&(lock->locked), false, memory_order_release);
}
