#ifndef _SEGMENT_H_
#define _SEGMENT_H_

#include <stdint.h>
#include <stdlib.h>

typedef struct {
  uint64_t canary;
  size_t size;
  size_t pow2_exp;
  void *control; // TODO: control structure
  void *read;
  void *write;
} segment_t;

void cons_opaque_ptr(size_t exp, void **ptr);

void *get_opaque_ptr_seg(void *ptr);

void *get_opaque_ptr_word(void *ptr);

size_t fst_aligned_offset(size_t align);

#endif
