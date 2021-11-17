#include <pthread.h>
#include <stdatomic.h>

#include "batcher.h"

int get_batcher_epoch(batcher_t *b) { return b->counter; }

void enter_batcher(batcher_t *b) {
  if (b->remaining == 0) {
  }
}

void leave_batcher(batcher_t *b) {}
