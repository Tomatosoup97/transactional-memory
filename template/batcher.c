#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "batcher.h"
#include "common.h"
#include "segment.h"
#include "tm.h"

int get_batcher_epoch(batcher_t *b) { return b->counter; }

void init_batcher(batcher_t *b) {
  // TODO: add unlikely here, remove asserts?
  b->counter = 0;
  b->remaining = 0;
  b->blocked = 0;
  assert(pthread_mutex_init(&b->critsec, NULL) == 0);
  assert(pthread_cond_init(&b->waiters, NULL) == 0);
}

void enter_batcher(batcher_t *b) {
  if (DEBUG)
    printf("Entering batcher, rem: %d, counter: %d, blocked: %d\n",
           b->remaining, b->counter, b->blocked);
  // TODO: rethink critsec for optimization purposes
  assert(pthread_mutex_lock(&b->critsec) == 0);

  if (!CAS(&b->remaining, 0, 1)) {
    atomic_fetch_add(&b->blocked, 1);
    pthread_cond_wait(&b->waiters, &b->critsec);
  }

  assert(pthread_mutex_unlock(&b->critsec) == 0);
}

void epoch_cleanup(struct region_s *region) {
  link_t *link = region->seg_links->next;

  while (true) {
    segment_t *seg = link->seg;
    size_t words_count = seg->size / region->align;
    /* size_t fst_aligned = fst_aligned_offset(region->align); */
    size_t control_size = words_count * sizeof(control_t);

    // TODO: handle read, write, dealloc, free etc
    memset(seg->control, 0, control_size);

    SEG_CANARY_CHECK(seg);

    bool is_last = link == region->seg_links;
    if (is_last)
      break;
    link = link->next;
  }
}

void leave_batcher(struct region_s *region) {
  batcher_t *b = region->batcher;
  if (DEBUG)
    printf("Leaving batcher, rem: %d, counter: %d, blocked: %d\n", b->remaining,
           b->counter, b->blocked);
  assert(pthread_mutex_lock(&b->critsec) == 0);

  atomic_fetch_add(&b->remaining, -1);

  if (CAS(&b->remaining, 0, b->blocked)) {
    // TODO: do I need to call leave when write fails?
    // TODO: clean up here
    atomic_fetch_add(&b->counter, 1);
    b->blocked = 0;
    epoch_cleanup(region);
    pthread_cond_broadcast(&b->waiters);
  }

  assert(pthread_mutex_unlock(&b->critsec) == 0);
}
