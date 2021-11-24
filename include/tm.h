#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "batcher.h"
#include "link.h"
#include "segment.h"

typedef struct region_s {
  size_t size;
  size_t align;
  batcher_t *batcher;
  link_t *seg_links;
  link_t *dirty_seg_links;
} region_t;

typedef void *shared_t;
static shared_t const invalid_shared = NULL;

typedef int alloc_t;
static alloc_t const success_alloc = 0;
static alloc_t const abort_alloc = 1;
static alloc_t const nomem_alloc = 2;

void move_to_clean(region_t *region, segment_t *seg);
void move_to_dirty(region_t *region, segment_t *seg);

// Interface

shared_t tm_create(size_t, size_t);
void tm_destroy(shared_t);
void *tm_start(shared_t);
size_t tm_size(shared_t);
size_t tm_align(shared_t);
tx_t tm_begin(shared_t, bool);
bool tm_end(shared_t, tx_t);
bool tm_read(shared_t, tx_t, void const *, size_t, void *);
bool tm_write(shared_t, tx_t, void const *, size_t, void *);
alloc_t tm_alloc(shared_t, tx_t, size_t, void **);
bool tm_free(shared_t, tx_t, void *);
