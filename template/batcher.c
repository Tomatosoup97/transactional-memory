#define _GNU_SOURCE

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
  b->counter = 0;
  b->remaining = 0;
  b->blocked = 0;
  assert(pthread_mutex_init(&b->critsec, NULL) == 0);
  assert(pthread_cond_init(&b->waiters, NULL) == 0);
}

void enter_batcher(batcher_t *b) {
  // TODO: rethink critsec for optimization purposes

  if (DEBUG1)
    printf("Entering batcher, rem: %d, counter: %d, blocked: %d\n",
           b->remaining, b->counter, b->blocked);

  assert(pthread_mutex_lock(&b->critsec) == 0);

  if (!CAS(&b->remaining, 0, 1)) {
    {
      atomic_fetch_add(&b->blocked, 1);
      pthread_cond_wait(&b->waiters, &b->critsec);
    }
  }
  assert(pthread_mutex_unlock(&b->critsec) == 0);
}

void epoch_cleanup(struct region_s *region) {
  if (region->dirty_seg_links == NULL) {
    // Nothing to clean up
    return;
  }
  link_t *link = region->dirty_seg_links->next;
  link_t *next = NULL;
  size_t align = region->align;

  while (true) {
    bool is_last = link == region->dirty_seg_links;

    if (!is_last) {
      assert(link->prev != link);
      assert(link->next != link);
    }

    /* printf("Current:  %p\n", link); */
    /* printf("Next   :  %p\n", link->next); */
    /* printf("First  :  %p\n", region->dirty_seg_links); */
    next = link->next;
    segment_t *seg = link->seg;
    size_t words_count = seg->size / align;
    size_t control_size = words_count * sizeof(control_t);
    SEG_CANARY_CHECK(seg);

    bool success_free = (!seg->rollback) && seg->should_free;
    bool failure_alloc = seg->rollback && seg->newly_alloc;

    if (success_free || failure_alloc) {
      if (DEBUG1)
        printf("[%p] Batcher: Freeing segment\n", seg);
      link_remove(&region->dirty_seg_links, &seg->link, false, true);
      free_segment(seg);
    } else {
      if (DEBUG1)
        printf("[%p] Batcher: Commiting segment\n", seg);

      memset(seg->control, 0, control_size);
      memcpy(seg->read, seg->write, seg->size);

      move_to_clean(region, seg);

      seg->owner = 0;
      seg->newly_alloc = false;
      seg->should_free = false;
      seg->dirty = false;
      seg->rollback = false;
    }

    if (is_last)
      break;
    link = next;
  }

  /* if (region->dirty_seg_links != NULL) { */
  /*   link_append(&region->seg_links, region->dirty_seg_links); */
  /*   region->dirty_seg_links = NULL; */
  /* } */

  if (DEBUG1)
    printf("=== End of epoch ===\n");
}

void leave_batcher(struct region_s *region) {
  batcher_t *b = region->batcher;

  if (DEBUG1)
    printf("Leaving batcher, rem: %d, counter: %d, blocked: %d\n", b->remaining,
           b->counter, b->blocked);

  assert(pthread_mutex_lock(&b->critsec) == 0);
  atomic_fetch_add(&b->remaining, -1);

  if (CAS(&b->remaining, 0, b->blocked)) {
    /* printf("Preparing for a new epoch\n"); */
    epoch_cleanup(region);
    b->blocked = 0;
    atomic_fetch_add(&b->counter, 1);
    pthread_cond_broadcast(&b->waiters);
  }

  assert(pthread_mutex_unlock(&b->critsec) == 0);
}
