#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#ifdef __STDC_NO_ATOMICS__
#error Current C11 compiler does not support atomic operations
#endif

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "batcher.h"
#include "common.h"
#include "link.h"
#include "lock.h"
#include "segment.h"
#include "tm.h"

static atomic_int trans_counter = 1;

shared_t tm_create(size_t size, size_t align) {
  if (DEBUG1)
    printf("TM create, size: %ld, align: %ld\n", size, align);
  region_t *region = (region_t *)malloc(sizeof(region_t));
  batcher_t *batcher = (batcher_t *)malloc(sizeof(batcher_t));
  region->seg_links = NULL;
  region->dirty_seg_links = NULL;
  assert(pthread_mutex_init(&region->lock, NULL) == 0);
  segment_t *seg = NULL;

  init_batcher(batcher);

  if (unlikely(!region)) {
    return invalid_shared;
  }

  if (unlikely(alloc_segment(&seg, align, size, 0) != 0)) {
    return invalid_shared;
  }

  seg->dirty = false;
  seg->newly_alloc = false; // newly_alloc is only defined within transaction
  link_insert(&region->seg_links, seg, false);

  region->batcher = batcher;
  region->align = align;
  region->start = seg;
  return region;
}

void tm_destroy(shared_t shared) {
  if (DEBUG1)
    printf("TM destroy\n");
  region_t *region = (region_t *)shared;
  link_t *link = region->seg_links->next;

  while (true) {
    bool is_last = link == region->seg_links;
    link_t *next = link->next;
    segment_t *seg = link->seg;

    if (DEBUG1)
      printf("[%p] Freeing segment\n", seg);

    link_remove(&region->seg_links, &link, false, true);
    SEG_CANARY_CHECK(seg);
    free_segment(seg);

    if (!OPT) {
      assert(region->dirty_seg_links == NULL);
    }

    if (is_last)
      break;
    link = next;
  }

  free(region->batcher);
  free(region);
}

void *tm_start(shared_t shared) {
  return cons_opaque_ptr_for_seg(((region_t *)shared)->start);
}

size_t tm_size(shared_t shared) {
  return ((region_t *)shared)->seg_links->seg->size;
}

size_t tm_align(shared_t shared) { return ((region_t *)shared)->align; }

tx_t tm_begin(shared_t shared, bool is_ro) {
  int tx_count = atomic_fetch_add(&trans_counter, 1);
  tx_t tx = tx_count;

  region_t *region = (region_t *)shared;

  if (is_ro) {
    tx |= read_only_tx;
  }
  if (DEBUG1)
    printf("[%lx] TM begin\n", tx);

  enter_batcher(region->batcher);
  return tx;
}

bool tm_end(shared_t shared, tx_t tx as(unused)) {
  if (DEBUG1)
    printf("[%lx] TM end\n", tx);
  region_t *region = (region_t *)shared;

  leave_batcher(region);
  return true;
}

void rollback_transaction(region_t *region, tx_t tx) {
  if (DEBUG1)
    printf("[%lx] Rolling back transaction\n", tx);

  if (region->dirty_seg_links == NULL) {
    // Nothing to rollback
    return;
  }
  link_t *link = region->dirty_seg_links->next;
  size_t align = region->align;
  link_t *base = region->dirty_seg_links;

  while (true) {
    bool is_last = link == base;
    segment_t *seg = link->seg;
    size_t words_count = seg->size / align;
    SEG_CANARY_CHECK(seg);

    if (seg->owner == tx) {
      seg->rollback = true;
    }

    for (size_t i = 0; i < words_count; i++) {
      spinlock_acquire(&seg->control[i].lock);
      if (seg->control[i].written && seg->control[i].access == tx) {
        uint64_t offset = i * align;
        memcpy(seg->write + offset, seg->read + offset, align);
        seg->control[i].written = 0;
        seg->control[i].access = 0;
      }
      spinlock_release(&seg->control[i].lock);
    }

    if (is_last)
      break;
    link = link->next;
  }
}

bool can_read_word(tx_t tx, segment_t *seg, uint64_t word_count) {
  if (VERBOSE_V2)
    printf("[%lx] read word %ld, access: %lx, many: %d\n", tx, word_count,
           seg->control[word_count].access,
           seg->control[word_count].many_accesses);

  if (seg->control[word_count].written) {
    if (likely(seg->control[word_count].access == tx)) {
      return true;
    } else {
      return false;
    }
  } else {
    CAS(&seg->control[word_count].access, 0, tx);

    if (seg->control[word_count].access != tx) {
      seg->control[word_count].many_accesses = true;
    }
    return true;
  }
}

bool _tm_read(region_t *region, tx_t tx, size_t size, void *target,
              segment_t *seg, size_t read_offset) {
  uint64_t align = region->align;
  uint64_t word_count = read_offset / align;

  if (unlikely(seg->newly_alloc && seg->owner != tx)) {
    return false;
  }

  if (is_tx_readonly(tx)) {
    memcpy(target, seg->read + read_offset, size);
    return true;
  } else {
    uint64_t offset = 0;

    while (offset < size) {
      spinlock_acquire(&seg->control[word_count].lock);
      bool success = can_read_word(tx, seg, word_count);
      spinlock_release(&seg->control[word_count].lock);
      if (unlikely(!success)) {
        return false;
      }
      offset += align;
      word_count++;
    }
    return true;
  }
}

bool tm_read(shared_t shared, tx_t tx, void const *source, size_t size,
             void *target) {
  region_t *region = (region_t *)shared;

  void *word = get_opaque_ptr_word((void *)source);
  segment_t *seg = (segment_t *)get_opaque_ptr_seg_word((void *)source, word);
  size_t read_offset = get_word_seg_offset(word, seg);

  bool res = _tm_read(region, tx, size, target, seg, read_offset);

  if (VERBOSE_V2)
    printf("[%lx] TM read - %.30s\n", tx, res ? (char *)target : "failure");

  bool is_readonly = is_tx_readonly(tx);

  if (res) {
    if (!is_readonly) {
      memcpy(target, seg->write + read_offset, size);
      move_to_dirty(region, seg);
    }
  } else {
    rollback_transaction(region, tx);
    leave_batcher(region);
  }

  return res;
}

bool can_write_word(region_t *region as(unused), tx_t tx, segment_t *seg,
                    uint64_t word_count) {
  if (VERBOSE_V2)
    printf("[%lx] write word %ld, access: %lx, many: %d\n", tx, word_count,
           seg->control[word_count].access,
           seg->control[word_count].many_accesses);

  if (seg->control[word_count].written) {
    if (seg->control[word_count].access == tx) {
      if (seg->control[word_count].many_accesses) {
        // TODO: that shouldnt' be necessary
        return false;
      }
      return true;
    } else {
      return false;
    }
  } else {
    CAS(&seg->control[word_count].access, 0, tx);

    if (seg->control[word_count].access == tx &&
        (!seg->control[word_count].many_accesses)) {
      seg->control[word_count].written = true;
      return true;
    } else {
      return false;
    }
  }
}

bool _tm_write(region_t *region, tx_t tx, void const *source, size_t size,
               segment_t *seg, size_t write_offset) {
  if (seg->newly_alloc && seg->owner != tx) {
    return false;
  }
  uint64_t align = region->align;
  uint64_t word_count = write_offset / align;
  void *actual_target = seg->write + write_offset;

  if (write_offset > seg->size) {
    return false;
  }

  uint64_t offset = 0;
  while (offset < size) {
    spinlock_acquire(&seg->control[word_count].lock);
    bool success = can_write_word(region, tx, seg, word_count);
    spinlock_release(&seg->control[word_count].lock);

    if (!success) {
      return false;
    }
    memcpy(actual_target + offset, source + offset, align);
    offset += align;
    word_count++;
  }
  return true;
}

bool tm_write(shared_t shared, tx_t tx, void const *source, size_t size,
              void *target) {
  region_t *region = (region_t *)shared;

  void *word = get_opaque_ptr_word((void *)target);
  segment_t *seg = (segment_t *)get_opaque_ptr_seg_word((void *)target, word);
  size_t write_offset = get_word_seg_offset(word, seg);

  if (VERBOSE)
    printf("[%lx] TM writing %.30s\n", tx, (char *)source);

  bool res = _tm_write(region, tx, source, size, seg, write_offset);

  if (VERBOSE)
    printf("[%lx] TM write - %s\n", tx, res ? "success" : "failure");

  if (res) {
    move_to_dirty(region, seg);
  } else {
    rollback_transaction(region, tx);
    leave_batcher(region);
  }

  return res;
}

alloc_t tm_alloc(shared_t shared, tx_t tx, size_t size, void **target) {
  if (DEBUG1)
    printf("[%lx] TM alloc\n", tx);
  region_t *region = (region_t *)shared;
  segment_t *segment = NULL;

  if (unlikely(alloc_segment(&segment, region->align, size, tx) != 0)) {
    return nomem_alloc;
  }

  link_insert(&region->dirty_seg_links, segment, false);

  *target = cons_opaque_ptr_for_seg(segment);
  return success_alloc;
}

bool tm_free(shared_t shared, tx_t tx, void *target) {
  if (DEBUG1)
    printf("[%lx] TM free\n", tx);
  region_t *region = (region_t *)shared;
  segment_t *seg = (segment_t *)get_opaque_ptr_seg(target);
  seg->should_free = true;
  seg->owner = tx;
  move_to_dirty(region, seg);
  return true;
}
