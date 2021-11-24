#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "segment.h"

bool is_tx_readonly(tx_t tx) { return (tx & read_only_tx) > 0; }

void free_segment(segment_t *segment) { free(segment); }

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

void cons_opaque_ptr(size_t exp, void **ptr) {
  *ptr = (void *)((uint64_t)(*ptr) | exp << 48);
}

void *get_opaque_ptr_word(void *ptr) {
  uint64_t clear_exp = ((uint64_t)1 << 48) - 1;
  uint64_t word_ptr = (uint64_t)ptr & clear_exp;
  return (void *)word_ptr;
}

size_t get_opaque_ptr_word_offset(void *ptr) {
  void *word = get_opaque_ptr_word(ptr);
  segment_t *seg = (segment_t *)get_opaque_ptr_seg(ptr);
  return word - seg->read;
};

void *get_opaque_ptr_seg(void *ptr) {
  size_t exp = (size_t)(uint64_t)ptr >> 48;
  ptr = get_opaque_ptr_word(ptr);
  uint64_t mask = UINT64_MAX ^ ((1 << exp) - 1);
  uint64_t seg_ptr = (uint64_t)ptr & mask;
  return (void *)seg_ptr;
}

size_t fst_aligned_offset(size_t align) {
  size_t ratio = sizeof(segment_t) / align;
  size_t is_multiple = sizeof(segment_t) % align == 0 ? 0 : 1;
  return ratio * align + align * is_multiple;
}

void *cons_opaque_ptr_for_seg(segment_t *seg) {
  void *ptr = seg->read;
  cons_opaque_ptr(seg->pow2_exp, &ptr);
  return ptr;
}
