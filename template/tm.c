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
#include "segment.h"
#include "tm.h"

#define MAX_SEGMENTS_COUNT 65536

static atomic_int trans_counter = 1;

int alloc_segment(segment_t **segment, size_t align, size_t size, tx_t tx) {
  if (DEBUG)
    printf("Allocating segment\n");
  size_t words_count = size / align;
  size_t fst_aligned = fst_aligned_offset(align);
  size_t control_size = words_count * sizeof(control_t);
  size_t seg_size = fst_aligned + control_size + size * 2;
  size_t alloc = next_pow2(seg_size); // TODO: we calculate pow2_exp twice

  if (unlikely(posix_memalign((void **)segment, alloc, alloc) != 0)) {
    return 1;
  }

  (*segment)->owner = tx;
  (*segment)->newly_alloc = true;
  (*segment)->should_free = false;
  (*segment)->dirty = true;
  (*segment)->rollback = false;
  (*segment)->size = size;
  (*segment)->pow2_exp = pow2_exp(seg_size);
  (*segment)->control = (control_t *)((void *)(*segment) + fst_aligned);
  (*segment)->read = (void *)((*segment)->control) + control_size;
  (*segment)->write = (*segment)->read + size;

  SET_SEG_CANARY((*segment));

  memset((*segment)->control, 0, control_size);
  memset((*segment)->read, 0, size);
  memset((*segment)->write, 0, size);
  return 0;
}

void free_segment(segment_t *segment) { free(segment); }

shared_t tm_create(size_t size, size_t align) {
  if (DEBUG)
    printf("TM create, size: %ld, align: %ld\n", size, align);
  region_t *region = (region_t *)malloc(sizeof(region_t));
  batcher_t *batcher = (batcher_t *)malloc(sizeof(batcher_t));
  region->seg_links = NULL;
  region->dirty_seg_links = NULL;
  segment_t *seg = NULL;

  init_batcher(batcher);

  if (unlikely(!region)) {
    return invalid_shared;
  }

  if (unlikely(alloc_segment(&seg, align, size, 0) != 0)) {
    return invalid_shared;
  }

  seg->dirty = false;
  link_insert(&region->seg_links, seg);

  region->batcher = batcher;
  region->align = align;
  return region;
}

void tm_destroy(shared_t shared) {
  if (DEBUG)
    printf("TM destroy\n");
  region_t *region = (region_t *)shared;
  link_t *link = region->seg_links->next;

  while (true) {
    bool is_last = link == region->seg_links;
    link_t *next = link->next;
    segment_t *seg = link->seg;
    link_remove(&region->seg_links, &link);
    // TODO: disable canary checks in actual solution
    SEG_CANARY_CHECK(seg);
    free_segment(seg);
    // TODO: disable check in actual solution
    assert(region->dirty_seg_links == NULL);

    if (is_last)
      break;
    link = next;
  }

  free(region->batcher);
  free(region);
}

void *tm_start(shared_t shared) {
  segment_t *seg = ((region_t *)shared)->seg_links->seg;
  return cons_opaque_ptr_for_seg(seg);
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
  if (DEBUG)
    printf("[%lx] TM begin\n", tx);

  enter_batcher(region->batcher);
  return tx;
}

bool tm_end(shared_t shared, tx_t tx as(unused)) {
  if (DEBUG)
    printf("[%lx] TM end\n", tx);
  region_t *region = (region_t *)shared;

  leave_batcher(region);
  return true; // TODO
}

bool read_word(tx_t tx, segment_t *seg, size_t align, void const *source,
               void *target, uint64_t offset, uint64_t word_count) {
  if (VERBOSE)
    printf("[%ld] word %ld, %ld access: %lx\n", tx, offset / (uint64_t)align,
           word_count, seg->control[word_count].access);

  // TODO: make sure that it's atomic w.r.t to write_word
  if (seg->control[word_count].written) {
    if (seg->control[word_count].access == tx) {
      memcpy(target + offset, source + offset, align);
      return true;
    } else {
      return false;
    }
  } else {
    CAS(&seg->control[word_count].access, 0, tx);
    // TODO: if someone writes here it should fail
    memcpy(target + offset, source + offset, align);

    if (!(seg->control[word_count].access == tx)) {
      seg->control[word_count].many_accesses = true;
    }
    return true;
  }
}

bool _tm_read(region_t *region, tx_t tx, void const *source, size_t size,
              void *target) {
  segment_t *seg = (segment_t *)get_opaque_ptr_seg((void *)source);
  size_t read_offset = get_opaque_ptr_word_offset((void *)source);
  uint64_t word_count = read_offset / region->align;

  if (is_tx_readonly(tx)) {
    memcpy(target, seg->read + read_offset, size);
    return true;
  } else {
    uint64_t offset = 0;
    while (offset < size) {
      if (!read_word(tx, seg, region->align, seg->write + read_offset, target,
                     offset, word_count)) {
        return false;
      }
      offset += region->align;
      word_count++;
    }
    return true;
  }
}

bool tm_read(shared_t shared, tx_t tx, void const *source, size_t size,
             void *target) {
  region_t *region = (region_t *)shared;

  /* if (VERBOSE) */
  /*   printf("[%lx] TM read \n", tx); */

  bool res = _tm_read(region, tx, source, size, target);

  /* if (VERBOSE) */
  /*   printf("[%lx] TM read - %s\n", tx, res ? "success" : "failure"); */

  if (!res)
    leave_batcher(region);

  return res;
}

bool write_word(tx_t tx, segment_t *seg, size_t align, void const *source,
                void *target, uint64_t offset, uint64_t word_count) {
  if (VERBOSE)
    printf("[%ld] word %ld, %ld access: %lx\n", tx, offset / (uint64_t)align,
           word_count, seg->control[word_count].access);

  if (seg->control[word_count].written) {
    if (seg->control[word_count].access == tx) {
      memcpy(target + offset, source + offset, align);
      return true;
    } else {
      return false;
    }
  } else {
    CAS(&seg->control[word_count].access, 0, tx);

    if (seg->control[word_count].access == tx &&
        (!seg->control[word_count].many_accesses)) {
      // TODO: atomic?
      memcpy(target + offset, source + offset, align);
      seg->control[word_count].written = true;
      return true;
    } else {
      return false;
    }
  }
}

bool _tm_write(region_t *region, tx_t tx, void const *source, size_t size,
               void *target) {
  segment_t *seg = (segment_t *)get_opaque_ptr_seg((void *)target);
  size_t write_offset = get_opaque_ptr_word_offset((void *)target);
  uint64_t word_count = write_offset / region->align;

  uint64_t offset = 0;
  while (offset < size) {
    if (!write_word(tx, seg, region->align, source, seg->write + write_offset,
                    offset, word_count)) {
      return false;
    }
    offset += region->align;
    word_count++;
  }
  return true;
}

void move_to_clean(region_t *region, segment_t *seg) {
  if (CAS(&seg->dirty, 1, 0)) {
    if (DEBUG)
      printf("[%p] Moving to clean\n", seg);
    link_remove(&region->dirty_seg_links, &seg->link);
    link_insert(&region->seg_links, seg);
  }
}

void move_to_dirty(region_t *region, segment_t *seg) {
  if (CAS(&seg->dirty, 0, 1)) {
    if (DEBUG)
      printf("[%p] Moving to dirty\n", seg);
    link_remove(&region->seg_links, &seg->link);
    link_insert(&region->dirty_seg_links, seg);
  }
}

void rollback_transaction(region_t *region, tx_t tx) {
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
      if (seg->control[i].written && seg->control[i].access == tx) {
        uint64_t offset = i * align;
        memcpy(seg->write + offset, seg->read + offset, align);
      }
    }

    if (is_last)
      break;
    link = link->next;
  }
}

bool tm_write(shared_t shared, tx_t tx, void const *source, size_t size,
              void *target) {
  region_t *region = (region_t *)shared;
  segment_t *seg = (segment_t *)get_opaque_ptr_seg((void *)target);

  if (VERBOSE)
    printf("[%lx] TM writing \n", tx);

  bool res = _tm_write(region, tx, source, size, target);

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
  if (DEBUG)
    printf("[%lx] TM alloc\n", tx);
  region_t *region = (region_t *)shared;
  segment_t *segment = NULL;

  if (unlikely(alloc_segment(&segment, region->align, size, tx) != 0)) {
    return nomem_alloc;
  }

  link_insert(&region->dirty_seg_links, segment);
  *target = cons_opaque_ptr_for_seg(segment);
  return success_alloc;
}

bool tm_free(shared_t shared, tx_t tx, void *target) {
  if (DEBUG)
    printf("[%lx] TM free\n", tx);
  region_t *region = (region_t *)shared;
  segment_t *seg = (segment_t *)get_opaque_ptr_seg(target);
  seg->should_free = true;
  seg->owner = tx;
  move_to_dirty(region, seg);
  return true;
}
