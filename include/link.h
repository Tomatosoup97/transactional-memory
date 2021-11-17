#ifndef _LINK_H_
#define _LINK_H_

typedef struct Link {
  struct Link *prev;
  struct Link *next;
} link_t;

void link_init(link_t *link);

void link_insert(link_t *link, link_t *base);

void link_remove(link_t *link);

#endif
