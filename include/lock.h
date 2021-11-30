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

bool spinlock_init(spinlock_t *lock);

bool spinlock_acquire(spinlock_t *lock);

void spinlock_release(spinlock_t *lock);

bool rwlock_init(rwlock_t *lock);

void rwlock_cleanup(rwlock_t *lock);

bool rwlock_acquire_writer(rwlock_t *lock);

bool rwlock_acquire_reader(rwlock_t *lock);

void rwlock_release(rwlock_t *lock);

#endif
