#ifndef _LOCK_H_
#define _LOCK_H_

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  atomic_bool locked;
} spinlock_t;

bool spinlock_init(spinlock_t *lock);

bool spinlock_acquire(spinlock_t *lock);

void spinlock_release(spinlock_t *lock);

#endif
