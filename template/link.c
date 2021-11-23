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
    printf("Inserting link\n");

  link_t *link = (link_t *)malloc(sizeof(link_t));
  link->seg = seg;
  seg->link = link;

  if (*base == NULL) {
    *base = link;
    (*base)->prev = *base;
    (*base)->next = *base;
    printf("base %p\n", (void *)(*base));
    printf("base seg %ld\n", (*base)->seg->size);
  }

  pthread_mutex_lock(&link_lock);
  {
    link_t *last = (*base)->prev;
    link->prev = last;
    link->next = *base;
    (*base)->prev = link;
    last->next = link;
  }
  pthread_mutex_unlock(&link_lock);
  if (DEBUG)
    printf("Inserted\n");
}

void link_remove(link_t *link) {
  if (DEBUG)
    printf("Removing link\n");
  pthread_mutex_lock(&link_lock);
  {
    link_t *prev = link->prev;
    link_t *next = link->next;
    prev->next = next;
    next->prev = prev;
  }
  pthread_mutex_unlock(&link_lock);

  free(link);
}
