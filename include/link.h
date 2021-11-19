#ifndef _LINK_H_
#define _LINK_H_

#include "segment.h"

typedef struct Link {
  segment_t *seg;
  struct Link *prev;
  struct Link *next;
} link_t;

void link_init(link_t *link, segment_t *seg);

void link_insert(link_t *base, segment_t *seg);

void link_remove(link_t *link);

#endif
