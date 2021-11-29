#ifndef _LINK_H_
#define _LINK_H_

#include "segment.h"

struct region_s;

typedef struct Link {
  segment_t *seg;
  struct Link *prev;
  struct Link *next;
} link_t;

void move_to_clean(struct region_s *region, segment_t *seg);

void move_to_dirty(struct region_s *region, segment_t *seg);

void link_insert(link_t **base, segment_t *seg, bool lock_taken);

void _link_insert(link_t **base, link_t *link, bool lock_taken);

void link_append(link_t **base, link_t *link);

void link_remove(link_t **base, link_t **link, bool lock_taken, bool discard);

#endif
