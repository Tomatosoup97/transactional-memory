#ifndef _LINK_H_
#define _LINK_H_

#include <stdint.h>

typedef struct {
  uint64_t canary;
  size_t size;
  size_t pow2_exp;
  void *read;
  void *write;
  void *control; // TODO: control structure
} segment_t;

typedef struct Link {
  segment_t *seg;
  struct Link *prev;
  struct Link *next;
} link_t;

void link_init(link_t *link, segment_t *seg);

void link_insert(segment_t *seg, link_t *base);

void link_remove(link_t *link);

#endif
