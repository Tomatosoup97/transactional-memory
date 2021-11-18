#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>

#include "common.h"
#include "minunit.h"
#include "tm.h"

void test_setup(void) {
  // Do nothing
}

void test_teardown(void) {
  // Do nothing
}

MU_TEST(test_allocate_segment) {
  segment_t *segment = (segment_t *)malloc(sizeof(segment_t));

  alloc_segment(segment, 8, 48);
  mu_check(segment->pow2_exp = 6);
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
}

int main() {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  return 0;
}
