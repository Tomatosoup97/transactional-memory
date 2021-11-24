#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "link.h"
#include "segment.h"

pthread_mutex_t link_lock = PTHREAD_MUTEX_INITIALIZER;

// TODO: this lock-full approach is definitely slow

void link_insert(link_t **base, segment_t *seg) {
  if (DEBUG)
    printf("[%p] Inserting link into %p\n", seg, (void *)*base);

  link_t *link = (link_t *)malloc(sizeof(link_t));
  link->seg = seg;
  seg->link = link;

  pthread_mutex_lock(&link_lock);
  {
    if (*base == NULL || (*base)->prev == NULL) {
      *base = link;
      (*base)->prev = *base;
      (*base)->next = *base;
    }

    link_t *last = (*base)->prev;
    link->prev = last;
    link->next = *base;
    (*base)->prev = link;
    last->next = link;
  }
  pthread_mutex_unlock(&link_lock);
  if (VERBOSE)
    printf("[%p] Inserted %p\n", seg, (void *)link);
}

void link_remove(link_t **base, link_t **link) {
  if (DEBUG)
    printf("[%p] Removing link %p\n", (*link)->seg, (void *)*link);

  pthread_mutex_lock(&link_lock);
  {
    bool is_last = (*link)->prev == (*link);

    link_t *prev = (*link)->prev;
    link_t *next = (*link)->next;
    prev->next = next;
    next->prev = prev;
    free(*link);
    *link = NULL;

    if (is_last)
      *base = NULL;
  }
  pthread_mutex_unlock(&link_lock);

  if (VERBOSE)
    printf("Removed link %p\n", (void *)*link);
}
