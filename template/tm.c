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

static atomic_int trans_counter = 0;

bool is_tx_readonly(tx_t tx) { return (tx & read_only_tx) > 0; }

int alloc_segment(segment_t **segment, size_t align, size_t size) {
  size_t words_count = size / align;
  size_t fst_aligned = fst_aligned_offset(align);
  size_t control_size = words_count * sizeof(control_t);
  size_t seg_size = fst_aligned + control_size + size * 2;
  size_t alloc = next_pow2(seg_size); // TODO: we calculate pow2_exp twice

  // TODO: do we actually need to align the memory given to the user?
  // TODO: is it fine to align to the alloc size?

  if (unlikely(posix_memalign((void **)segment, alloc, alloc) != 0)) {
    return 1;
  }

  (*segment)->newly_alloc = true;
  (*segment)->should_free = false;
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
  region_t *region = (region_t *)malloc(sizeof(region_t));
  batcher_t *batcher = (batcher_t *)malloc(sizeof(batcher_t));
  region->seg_links = (link_t *)malloc(sizeof(link_t));
  segment_t *seg = NULL;

  init_batcher(batcher);

  if (unlikely(!region)) {
    return invalid_shared;
  }

  if (unlikely(alloc_segment(&seg, align, size) != 0)) {
    return invalid_shared;
  }

  link_init(region->seg_links, seg);

  region->batcher = batcher;
  region->align = align;
  return region;
}

void tm_destroy(shared_t shared) {
  region_t *region = (region_t *)shared;

  while (true) {
    link_t *link = region->seg_links->next;
    bool is_last = link == region->seg_links;

    segment_t *seg = link->seg;
    link_remove(link);
    SEG_CANARY_CHECK(seg);
    free_segment(seg);

    if (is_last)
      break;
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
  enter_batcher(region->batcher);

  if (is_ro) {
    tx |= read_only_tx;
  }
  return tx;
}

bool tm_end(shared_t shared, tx_t tx as(unused)) {
  // TODO
  region_t *region = (region_t *)shared;

  leave_batcher(region->batcher);
  return true; // TODO
}

bool read_word(tx_t tx, segment_t *seg, size_t align, void const *source,
               void *target, uint64_t offset) {
  uint64_t word_count = offset / align;

  // TODO: make sure that it's atomic w.r.t to write_word
  if (seg->control[word_count].written) {
    if (seg->control[word_count].access == tx) {
      memcpy(target + offset, source + offset, align);
      return true;
    } else {
      return false;
    }
  } else {
    memcpy(target + offset, source + offset, align);
    CAS(&seg->control[word_count].access, 0, tx);

    if (!(seg->control[word_count].access == tx)) {
      seg->control[word_count].many_accesses = true;
    }
    return true;
  }
}

bool tm_read(shared_t shared, tx_t tx, void const *source, size_t size,
             void *target) {
  region_t *region = (region_t *)shared;
  segment_t *seg = (segment_t *)get_opaque_ptr_seg((void *)source);
  size_t read_offset = get_opaque_ptr_word_offset((void *)source);

  if (is_tx_readonly(tx)) {
    memcpy(target, seg->read + read_offset, size);
    return true;
  } else {
    uint64_t offset = 0;
    while (offset < size) {
      if (!read_word(tx, seg, region->align, seg->write + read_offset, target,
                     offset)) {
        return false;
      }
      offset += region->align;
    }
    return true;
  }
}

bool tm_write(shared_t shared as(unused), tx_t tx as(unused),
              void const *source as(unused), size_t size as(unused),
              void *target as(unused)) {
  // TODO: tm_write(shared_t, tx_t, void const*, size_t, void*)
  return false;
}

alloc_t tm_alloc(shared_t shared, tx_t tx as(unused), size_t size,
                 void **target) {
  region_t *region = (region_t *)shared;
  segment_t *segment = NULL;

  if (unlikely(alloc_segment(&segment, region->align, size) != 0)) {
    return nomem_alloc;
  }

  link_insert(region->seg_links, segment);
  *target = cons_opaque_ptr_for_seg(segment);
  return success_alloc;
}

bool tm_free(shared_t shared as(unused), tx_t tx as(unused),
             void *target as(unused)) {
  segment_t *seg = (segment_t *)get_opaque_ptr_seg(target);
  seg->should_free = true;
  return true;
}
