#ifndef _SEGMENT_H_
#define _SEGMENT_H_

#define _GNU_SOURCE

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "lock.h"

#define read_only_tx ((UINTPTR_MAX >> 1) + 1)

struct Link;

typedef unsigned long tx_t;
static tx_t const invalid_tx = ~((tx_t)0);

// TODO: rethink atomicity of vars

typedef struct {
  atomic_ulong access;
  atomic_bool written;
  atomic_bool many_accesses;
  spinlock_t lock;
} control_t;

typedef struct {
  uint64_t canary;
  size_t size;
  size_t pow2_exp;
  atomic_ulong owner;
  struct Link *link;
  pthread_mutex_t lock;
  bool newly_alloc;
  atomic_bool should_free;
  atomic_bool dirty;
  atomic_bool rollback;
  control_t *control;
  void *read;
  void *write;
} segment_t;

void free_segment(segment_t *segment);

int alloc_segment(segment_t **segment, size_t align, size_t size, tx_t tx);

void cons_opaque_ptr(size_t exp, void **ptr);

inline bool is_tx_readonly(tx_t tx) { return (tx & read_only_tx) > 0; }

inline void *get_opaque_ptr_word(void *ptr) {
  uint64_t clear_exp = ((uint64_t)1 << 48) - 1;
  uint64_t word_ptr = (uint64_t)ptr & clear_exp;
  return (void *)word_ptr;
}

inline size_t get_word_seg_offset(void *word, segment_t *seg) {
  return word - seg->read;
}

inline void *get_opaque_ptr_seg_word(void *ptr, void *word) {
  size_t exp = (size_t)(uint64_t)ptr >> 48;
  uint64_t mask = UINT64_MAX ^ ((1 << exp) - 1);
  uint64_t seg_ptr = (uint64_t)word & mask;
  return (void *)seg_ptr;
}

inline void *get_opaque_ptr_seg(void *ptr) {
  void *word = get_opaque_ptr_word(ptr);
  return get_opaque_ptr_seg_word(ptr, word);
}

inline size_t get_opaque_ptr_word_offset(void *ptr) {
  void *word = get_opaque_ptr_word(ptr);
  segment_t *seg = (segment_t *)get_opaque_ptr_seg(ptr);
  return word - seg->read;
};

inline size_t fst_aligned_offset(size_t align) {
  size_t ratio = sizeof(segment_t) / align;
  size_t is_multiple = sizeof(segment_t) % align == 0 ? 0 : 1;
  return ratio * align + align * is_multiple;
}

inline void *cons_opaque_ptr_for_seg(segment_t *seg) {
  void *ptr = seg->read;
  cons_opaque_ptr(seg->pow2_exp, &ptr);
  return ptr;
}

#endif
