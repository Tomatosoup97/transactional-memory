#ifndef _LOCK_H_
#define _LOCK_H_

#define _GNU_SOURCE

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  atomic_bool locked;
} spinlock_t;

typedef pthread_rwlock_t rwlock_t;

inline bool spinlock_init(spinlock_t *lock) {
  atomic_init(&(lock->locked), false);
  return true;
}

bool spinlock_acquire(spinlock_t *lock);

inline void spinlock_release(spinlock_t *lock) {
  atomic_store_explicit(&(lock->locked), false, memory_order_release);
}

bool rwlock_init(rwlock_t *lock);

void rwlock_cleanup(rwlock_t *lock);

bool rwlock_acquire_writer(rwlock_t *lock);

bool rwlock_acquire_reader(rwlock_t *lock);

void rwlock_release(rwlock_t *lock);

#endif
