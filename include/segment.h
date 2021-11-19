#ifndef _SEGMENT_H_
#define _SEGMENT_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef uintptr_t tx_t;
static tx_t const invalid_tx = ~((tx_t)0);
static const tx_t read_only_tx = (UINTPTR_MAX >> 1) + 1;

typedef struct {
  tx_t access;
  bool written;
} control_t;

typedef struct {
  uint64_t canary;
  size_t size;
  size_t pow2_exp;
  bool newly_alloc;
  bool should_free;
  control_t *control;
  void *read;
  void *write;
} segment_t;

void cons_opaque_ptr(size_t exp, void **ptr);

void *get_opaque_ptr_seg(void *ptr);

void *get_opaque_ptr_word(void *ptr);

void *cons_opaque_ptr_for_seg(segment_t *seg);

size_t fst_aligned_offset(size_t align);

#endif
