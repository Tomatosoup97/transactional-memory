#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#ifdef __STDC_NO_ATOMICS__
#error Current C11 compiler does not support atomic operations
#endif

#include "tm.h"
#include "common.h"
#include <stdlib.h>

#define MAX_SEGMENTS_COUNT 65536

typedef struct {
  size_t size; // size of segment
  void *read;
  void *write;
  void *control; // TODO: control structure;
} segment_t;

typedef struct {
  size_t size;
  size_t align;                            // requested alignment
  size_t align_alloc;                      // actual allocation alignment
  segment_t *segments[MAX_SEGMENTS_COUNT]; // TODO: make it a list
} region_t;

int allocate_segment(segment_t *segment, size_t align_alloc, size_t size) {
  // allocate up to X

  if (unlikely(posix_memalign(&(segment->read), align_alloc, size) != 0)) {
    return 1;
  }

  if (unlikely(posix_memalign(&(segment->write), align_alloc, size) != 0)) {
    return 1;
  }
  segment->size = size;

  // memset(segment, 0, size);
  return 0;
}

// -------------------------------------------------------------------------- //

/** Create (i.e. allocate + init) a new shared memory region, with one first
 *non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in
 *bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared
 *memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
 **/
shared_t tm_create(size_t size as(unused), size_t align as(unused)) {
  region_t *region = (region_t *)malloc(sizeof(region_t));

  if (unlikely(!region)) {
    return invalid_shared;
  }

  size_t align_alloc = align < sizeof(void *) ? sizeof(void *) : align;

  if (unlikely(allocate_segment(region->segments[0], align_alloc, size) != 0)) {
    return invalid_shared;
  }

  region->align = align;
  region->align_alloc = align_alloc;

  return region;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
 **/
void tm_destroy(shared_t shared as(unused)) {
  // TODO: tm_destroy(shared_t)
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
  return ((region_t *)shared)->segments[0]->size;
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the
 *given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
 **/
size_t tm_align(shared_t shared as(unused)) {
  return ((region_t *)shared)->align;
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
 **/
tx_t tm_begin(shared_t shared as(unused), bool is_ro as(unused)) {
  // TODO: tm_begin(shared_t)
  return invalid_tx;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
 **/
bool tm_end(shared_t shared as(unused), tx_t tx as(unused)) {
  // TODO: tm_end(shared_t, tx_t)
  return false;
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

/** [thread-safe] Memory allocation in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param size   Allocation requested size (in bytes), must be a positive
 *multiple of the alignment
 * @param target Pointer in private memory receiving the address of the first
 *byte of the newly allocated, aligned segment
 * @return Whether the whole transaction can continue (success/nomem), or not
 *(abort_alloc)
 **/
alloc_t tm_alloc(shared_t shared as(unused), tx_t tx as(unused),
                 size_t size as(unused), void **target as(unused)) {
  // TODO: tm_alloc(shared_t, tx_t, size_t, void**)
  return abort_alloc;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment
 *to deallocate
 * @return Whether the whole transaction can continue
 **/
bool tm_free(shared_t shared as(unused), tx_t tx as(unused),
             void *target as(unused)) {
  // TODO: tm_free(shared_t, tx_t, void*)
  return false;
}
