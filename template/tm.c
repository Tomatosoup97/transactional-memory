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

int alloc_segment(segment_t **segment, size_t align, size_t size) {
  size_t fst_aligned = fst_aligned_offset(align);
  size_t seg_size = fst_aligned + size * 2; // TODO: + control size
  size_t alloc = next_pow2(seg_size);       // TODO: we calculate pow2_exp twice

  // TODO: do we actually need to align the memory given to the user?
  // TODO: is it fine to align to the alloc size?

  if (unlikely(posix_memalign((void **)segment, alloc, alloc) != 0)) {
    return 1;
  }

  (*segment)->size = size;
  (*segment)->pow2_exp = pow2_exp(seg_size);
  (*segment)->read = (void *)(*segment) + fst_aligned;
  (*segment)->write = (void *)(*segment) + fst_aligned + size;

  SET_SEG_CANARY((*segment));

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

void *tm_start(shared_t shared as(unused)) {
  // TODO: tm_start(shared_t)
  return NULL;
}

size_t tm_size(shared_t shared) {
  return ((region_t *)shared)->seg_links->seg->size;
}

size_t tm_align(shared_t shared) { return ((region_t *)shared)->align; }

tx_t tm_begin(shared_t shared, bool is_ro) {
  // TODO: rethink
  region_t *region = (region_t *)shared;
  enter_batcher(region->batcher);
  if (is_ro) {
    return read_only_tx;
  }
  return read_write_tx;
}

bool tm_end(shared_t shared, tx_t tx as(unused)) {
  // TODO
  region_t *region = (region_t *)shared;

  leave_batcher(region->batcher);
  return true; // TODO
}

bool tm_read(shared_t shared as(unused), tx_t tx as(unused),
             void const *source as(unused), size_t size as(unused),
             void *target as(unused)) {
  // TODO: tm_read(shared_t, tx_t, void const*, size_t, void*)
  return false;
}

bool tm_write(shared_t shared as(unused), tx_t tx as(unused),
              void const *source as(unused), size_t size as(unused),
              void *target as(unused)) {
  // TODO: tm_write(shared_t, tx_t, void const*, size_t, void*)
  return false;
}

alloc_t tm_alloc(shared_t shared, tx_t tx, size_t size, void **target) {
  region_t *region = (region_t *)shared;
  segment_t *segment = NULL;

  if (unlikely(alloc_segment(&segment, region->align, size) != 0)) {
    return nomem_alloc;
  }

  link_insert(region->seg_links, segment);

  // TODO: nooot exactly (?)
  if (tx == read_only_tx) {
    *target = segment->read;
  } else if (tx == read_write_tx) {
    *target = segment->write;
  }

  cons_opaque_ptr(segment->pow2_exp, target);
  return success_alloc;
}

bool tm_free(shared_t shared as(unused), tx_t tx as(unused),
             void *target as(unused)) {
  // TODO: tm_free(shared_t, tx_t, void*)
  return false;
}
