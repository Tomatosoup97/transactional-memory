#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#ifdef __STDC_NO_ATOMICS__
#error Current C11 compiler does not support atomic operations
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "batcher.h"
#include "common.h"
#include "link.h"
#include "tm.h"

#define MAX_SEGMENTS_COUNT 65536

int alloc_segment(segment_t *segment, size_t align, size_t size) {
  size_t actual_size = next_pow2(size); // TODO: we calculate powe2_exp twice

  if (unlikely(posix_memalign(&(segment->read), align, actual_size) != 0)) {
    return 1;
  }

  if (unlikely(posix_memalign(&(segment->write), align, actual_size) != 0)) {
    return 1;
  }

  segment->size = size;
  segment->pow2_exp = pow2_exp(size);
  SET_SEG_CANARY(segment);

  memset(segment->read, 0, actual_size);
  memset(segment->write, 0, actual_size);
  return 0;
}

void free_segment(segment_t *segment) {
  free(segment->read);
  free(segment->write);
  free(segment);
}

shared_t tm_create(size_t size as(unused), size_t align as(unused)) {
  region_t *region = (region_t *)malloc(sizeof(region_t));
  batcher_t *batcher = (batcher_t *)malloc(sizeof(batcher_t));
  segment_t *seg = (segment_t *)malloc(sizeof(segment_t));
  region->seg_links = (link_t *)malloc(sizeof(link_t));

  init_batcher(batcher);
  link_init(region->seg_links, seg);

  if (unlikely(!region)) {
    return invalid_shared;
  }

  size_t align_alloc = align < sizeof(void *) ? sizeof(void *) : align;

  if (unlikely(alloc_segment(seg, align_alloc, size) != 0)) {
    return invalid_shared;
  }

  region->batcher = batcher;
  region->align = align;
  region->align_alloc = align_alloc;

  return region;
}

void tm_destroy(shared_t shared as(unused)) {
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

/** [thread-safe] Return the start address of the first allocated segment in the
 *shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
 **/
void *tm_start(shared_t shared as(unused)) {
  // TODO: tm_start(shared_t)
  return NULL;
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of
 *the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
 **/
size_t tm_size(shared_t shared as(unused)) {
  return ((region_t *)shared)->seg_links->seg->size;
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the
 *given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
 **/
size_t tm_align(shared_t shared as(unused)) {
  return ((region_t *)shared)->align;
}

tx_t tm_begin(shared_t shared as(unused), bool is_ro as(unused)) {
  // TODO: rethink
  region_t *region = (region_t *)shared;
  enter_batcher(region->batcher);
  if (is_ro) {
    return read_only_tx;
  }
  return read_write_tx;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
 **/
bool tm_end(shared_t shared as(unused), tx_t tx as(unused)) {
  // TODO
  region_t *region = (region_t *)shared;

  leave_batcher(region->batcher);
  return true; // TODO
}

/** [thread-safe] Read operation in the given transaction, source in the shared
 *region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the
 *alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
 **/
bool tm_read(shared_t shared as(unused), tx_t tx as(unused),
             void const *source as(unused), size_t size as(unused),
             void *target as(unused)) {
  // TODO: tm_read(shared_t, tx_t, void const*, size_t, void*)
  return false;
}

/** [thread-safe] Write operation in the given transaction, source in a private
 *region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the
 *alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
 **/
bool tm_write(shared_t shared as(unused), tx_t tx as(unused),
              void const *source as(unused), size_t size as(unused),
              void *target as(unused)) {
  // TODO: tm_write(shared_t, tx_t, void const*, size_t, void*)
  return false;
}

/* [thread-safe] Memory allocation in the given transaction.
 * @return if the transaction can continue (success/nomem), or not (abort_alloc)
 */
alloc_t tm_alloc(shared_t shared as(unused), tx_t tx as(unused),
                 size_t size as(unused), void **target as(unused)) {

  region_t *region = (region_t *)shared;
  segment_t *segment = (segment_t *)malloc(sizeof(segment_t));

  if (unlikely(alloc_segment(segment, region->align_alloc, size) != 0)) {
    return nomem_alloc;
  }

  link_insert(segment, region->seg_links);

  // TODO: nooot exactly (?)
  if (tx == read_only_tx) {
    *target = segment->read;
  } else if (tx == read_write_tx) {
    *target = segment->write;
  }
  return success_alloc;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param target Address of the first byte of the previously allocated segment
 *to deallocate
 * @return Whether the whole transaction can continue
 **/
bool tm_free(shared_t shared as(unused), tx_t tx as(unused),
             void *target as(unused)) {
  // TODO: tm_free(shared_t, tx_t, void*)
  return false;
}
