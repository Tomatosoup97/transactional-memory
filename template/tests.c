#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>

#include "common.h"
#include "minunit.h"
#include "segment.h"
#include "tm.h"

void test_setup(void) {
  // Do nothing
}

void test_teardown(void) {
  // Do nothing
}

MU_TEST(test_opaque_ptr_arith) {
  // Test cons_opaque ptr
  void *ptr = (void *)0x00000deadbeef;
  cons_opaque_ptr(8, &ptr);
  mu_check((uint64_t)ptr == 0x80000deadbeef);

  ptr = (void *)0x00000deadbeef;
  cons_opaque_ptr(32, &ptr);
  mu_check((uint64_t)ptr == 0x200000deadbeef);

  // Test get_opaque_ptr_seg
  mu_check((uint64_t)get_opaque_ptr_seg((void *)0x80000deadbeef) ==
           0x00000deadbe00);
  mu_check((uint64_t)get_opaque_ptr_seg((void *)0x100000deadbeef) ==
           0x000000dead0000);

  // Test get_opaque_ptr_word

  mu_check((uint64_t)get_opaque_ptr_word((void *)0x80000deadbeef) ==
           0xdeadbeef);
  mu_check((uint64_t)get_opaque_ptr_word((void *)0x100000deadbeef) ==
           0xdeadbeef);

  // whole scenario
  void *seg = (void *)0x1739800;

  cons_opaque_ptr(8, &seg);
  mu_check((uint64_t)seg == 0x8000001739800);

  // Test fst_aligned_offset

  size_t s4 = fst_aligned_offset(4);
  size_t s32 = fst_aligned_offset(32);
  size_t s1024 = fst_aligned_offset(1024);

  mu_check(s4 >= sizeof(segment_t));
  mu_check(s32 >= sizeof(segment_t));
  mu_check(s1024 >= sizeof(segment_t));

  mu_check(s4 % 4 == 0);
  mu_check(s32 % 32 == 0);
  mu_check(s1024 % 1024 == 0);
}

MU_TEST(test_tm_alloc_opaque_ptr) {
  size_t align = 16;
  shared_t region_p = tm_create(64, align);
  region_t *region = ((region_t *)region_p);
  void *ptr;
  tx_t tx = tm_begin(region, true);
  tm_alloc(region, tx, 256, &ptr);
  {
    segment_t *seg = region->seg_links->next->seg;

    mu_check(get_opaque_ptr_seg(ptr) == seg);
    mu_check(get_opaque_ptr_word(ptr) == seg->read);

    ptr += 201;

    mu_check(get_opaque_ptr_seg(ptr) == seg);
    mu_check(get_opaque_ptr_word(ptr) == seg->read + 201);
    mu_check((uint64_t)seg->read % align == 0);
    mu_check((uint64_t)seg->write % align == 0);
  }
  tm_end(region, tx);
  tm_destroy(region);
}

MU_TEST(test_allocate_segment) {
  segment_t *segment = NULL;

  alloc_segment(&segment, 8, 48);

  mu_check(segment->pow2_exp == 8);
  mu_check(segment->size == 48);
  free_segment(segment);
}

MU_TEST(test_batcher_one_thread) {
  batcher_t *b = (batcher_t *)malloc(sizeof(batcher_t));
  init_batcher(b);

  enter_batcher(b);
  {
    mu_check(get_batcher_epoch(b) == 0);
    mu_check(b->remaining == 1);
  }
  leave_batcher(b);

  mu_check(get_batcher_epoch(b) == 1);
  mu_check(b->remaining == 0);
}

MU_TEST(test_mem_region) {
  shared_t region_p = tm_create(120, 8);
  region_t *region = ((region_t *)region_p);
  {
    mu_check(IS_SEG_VALID(region->seg_links->seg));
    mu_check(tm_size(region_p) == 120);
    mu_check(tm_align(region_p) == 8);
  }
  tm_destroy(region);
}

MU_TEST(test_transaction) {
  size_t align = 32;

  shared_t region_p = tm_create(align * 1, align);
  region_t *region = ((region_t *)region_p);
  {
    bool is_ro = false;
    tx_t tx = tm_begin(region, is_ro);
    {
      void *mem1, *mem2;
      alloc_t alloc_status = tm_alloc(region, tx, align * 2, &mem1);

      mu_check(alloc_status == success_alloc);
      mu_check(region->seg_links->seg->size == align * 1);
      mu_check(region->seg_links->next->seg->size == align * 2);
      mu_check(region->seg_links->next->next == region->seg_links);
      mu_check(region->seg_links->next->prev == region->seg_links);

      alloc_status = tm_alloc(region, tx, align * 3, &mem2);

      mu_check(alloc_status == success_alloc);
      mu_check(region->seg_links->prev->seg->size == align * 3);
    }
    tm_end(region, tx);
  }
  tm_destroy(region);
}

MU_TEST(test_pow_funcs) {
  mu_check(pow2_exp(31) == 5);
  mu_check(pow2_exp(48) == 6);
  mu_check(pow2_exp(64) == 6);

  mu_check(next_pow2(48) == 64);
  mu_check(next_pow2(63) == 64);
  mu_check(next_pow2(64) == 64);
  mu_check(next_pow2(65) == 128);
}

MU_TEST_SUITE(test_suite) {
  MU_RUN_TEST(test_pow_funcs);
  MU_RUN_TEST(test_allocate_segment);
  MU_RUN_TEST(test_batcher_one_thread);
  MU_RUN_TEST(test_mem_region);
  MU_RUN_TEST(test_transaction);
  MU_RUN_TEST(test_opaque_ptr_arith);
  MU_RUN_TEST(test_tm_alloc_opaque_ptr);
}

int main() {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  return 0;
}
