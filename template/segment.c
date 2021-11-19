#include <stdint.h>

#include "segment.h"

void cons_opaque_ptr(size_t exp, void **ptr) {
  *ptr = (void *)((uint64_t)(*ptr) | exp << 48);
}

void *get_opaque_ptr_word(void *ptr) {
  uint64_t clear_exp = ((uint64_t)1 << 48) - 1;
  uint64_t word_ptr = (uint64_t)ptr & clear_exp;
  return (void *)word_ptr;
}

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
