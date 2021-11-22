#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

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

MU_TEST(test_tm_read_write) {
  size_t align = 4;

  shared_t region_p = tm_create(align * 32, align);
  region_t *region = ((region_t *)region_p);
  {
    const size_t size = 13;
    char source[size];
    void *mem = tm_start(region);
    bool success;

    strcpy(source, "Hello memory");
    memcpy(region->seg_links->seg->read, source, size);
    memcpy(region->seg_links->seg->write, source, size);

    tx_t tx1 = tm_begin(region, false);
    {
      // Test read in read-write memory
      char target[size];
      success = tm_read(region, tx1, mem, size, target);

      mu_check(success);
      mu_check(strncmp(target, "Hello memory", size) == 0);

      char target2[size];
      mu_check(tm_read(region, tx1, mem, 8, target2));
      mu_check(strncmp(target2, "Hello me", 8) == 0);

      mem += 8;

      char target3[size];
      mu_check(tm_read(region, tx1, mem, 4, target3));
      mu_check(strncmp(target3, "mory", 4) == 0);
    }
    {
      // Test write in read-write memory
      char target[16];
      mem += 4;

      mu_check(tm_write(region, tx1, "Guy Fieri iscool", 16, mem));
      mu_check(tm_read(region, tx1, mem, 16, target));
      mu_check(strncmp(target, "Guy Fieri iscool", 16) == 0);

      mu_check(tm_write(region, tx1, "Reif", 4, mem + 4));
      mu_check(tm_read(region, tx1, mem, 16, target));
      mu_check(strncmp(target, "Guy Reifi iscool", 16) == 0);
    }
    tm_end(region, tx1);
    tx_t tx2 = tm_begin(region, false);
    {
      // Test overwrite fails in read-write memory
      mu_check(!tm_write(region, tx2, "Fieri is notcool", 16, mem));
    }
    tm_end(region, tx2);

    {
      // Test failed write is not reflected
      char target[16];
      mu_check(tm_read(region, tx1, mem, 16, target));
      mu_check(strncmp(target, "Guy Reifi iscool", 16) == 0);
    }

    tx_t tx1_ro = tm_begin(region, true);
    {
      // Test read in read-only memory
      mem = tm_start(region) + 4;

      char target[size];
      mu_check(tm_read(region, tx1_ro, mem, size, target));
      mu_check(strncmp(target, "o memory", size) == 0);
    }

    tm_end(region, tx1_ro);

    tx_t tx2_ro = tm_begin(region, true);
    {
      char target[size];
      mu_check(tm_read(region, tx2_ro, mem - 4, size, target));
      mu_check(strncmp(target, "Hello memory", size) == 0);
    }
    tm_end(region, tx2_ro);

    {
      // Test transaction numbers
      mu_check(tx1 < tx2);
      mu_check(tx1_ro < tx2_ro);

      mu_check(!is_tx_readonly(tx1));
      mu_check(is_tx_readonly(tx1_ro));
    }
  }
  tm_destroy(region);
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
  shared_t region_p = tm_create(32, 1);
  region_t *region = ((region_t *)region_p);
  batcher_t *b = region->batcher;

  enter_batcher(b);
  {
    mu_check(get_batcher_epoch(b) == 0);
    mu_check(b->remaining == 1);
  }
  leave_batcher(region);

  mu_check(get_batcher_epoch(b) == 1);
  mu_check(b->remaining == 0);

  tm_destroy(region);
}

#define thread_count 4

typedef struct {
  region_t *r;
  volatile bool stay;
} batcher_run_args_t;

void *batcher_runner(void *p) {
  batcher_run_args_t *args = (batcher_run_args_t *)p;

  enter_batcher(args->r->batcher);
  while (args->stay) {
    // spin
  }
  leave_batcher(args->r);
  return NULL;
}

MU_TEST(test_batcher_multi_thread) {
  shared_t region_p = tm_create(32, 1);
  region_t *region = ((region_t *)region_p);
  batcher_t *b = region->batcher;

  pthread_t threads[thread_count];
  batcher_run_args_t args[thread_count];

  for (int i = 0; i < thread_count; i++) {
    args[i].stay = true;
    args[i].r = region;
    pthread_create(&(threads[i]), NULL, batcher_runner, &args[i]);
  }

  sleep(0.5);

  {
    mu_check(get_batcher_epoch(b) == 0);
    mu_check(b->remaining == 1);
    mu_check(b->blocked == 3);
  }

  for (int i = 0; i < thread_count; i++) {
    args[i].stay = false;
  }

  for (int i = 0; i < thread_count; i++) {
    pthread_join(threads[i], NULL);
  }

  sleep(0.5);

  mu_check(get_batcher_epoch(b) == 2);
  mu_check(b->remaining == 0);
  mu_check(b->blocked == 0);
}

MU_TEST(test_mem_region) {
  shared_t region_p = tm_create(120, 32);
  region_t *region = ((region_t *)region_p);
  {
    mu_check(IS_SEG_VALID(region->seg_links->seg));
    mu_check(tm_size(region_p) == 120);
    mu_check(tm_align(region_p) == 32);

    mu_check(get_opaque_ptr_seg(tm_start(region_p)) == region->seg_links->seg);
  }
  /* shared_t region_p = tm_create(align * 32, align); */
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
  MU_RUN_TEST(test_mem_region);
  MU_RUN_TEST(test_transaction);
  MU_RUN_TEST(test_opaque_ptr_arith);
  MU_RUN_TEST(test_tm_alloc_opaque_ptr);
  MU_RUN_TEST(test_tm_read_write);
  MU_RUN_TEST(test_batcher_one_thread);
  MU_RUN_TEST(test_batcher_multi_thread);
}

int main() {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  return 0;
}
