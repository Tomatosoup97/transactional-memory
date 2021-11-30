#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "segment.h"

void free_segment(segment_t *segment) { free(segment); }

int alloc_segment(segment_t **segment, size_t align, size_t size, tx_t tx) {
  if (DEBUG)
    printf("Allocating segment\n");
  size_t words_count = size / align;
  size_t fst_aligned = fst_aligned_offset(align);
  size_t control_size = words_count * sizeof(control_t);
  size_t seg_size = fst_aligned + control_size + size * 2;
  size_t alloc = next_pow2(seg_size);

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
  assert(pthread_mutex_init(&(*segment)->lock, NULL) == 0);
  (*segment)->control = (control_t *)((void *)(*segment) + fst_aligned);
  (*segment)->read = (void *)((*segment)->control) + control_size;
  (*segment)->write = (*segment)->read + size;

  SET_SEG_CANARY((*segment));

  memset((*segment)->control, 0, control_size);
  memset((*segment)->read, 0, size);
  memset((*segment)->write, 0, size);
  return 0;
}

void cons_opaque_ptr(size_t exp, void **ptr) {
  *ptr = (void *)((uint64_t)(*ptr) | exp << 48);
}
