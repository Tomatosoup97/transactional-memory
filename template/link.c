#include <pthread.h>

#include <stdlib.h>

#include "link.h"

pthread_mutex_t link_lock = PTHREAD_MUTEX_INITIALIZER;

// TODO: this lock-full approach is definitely slow

void link_init(link_t *link, segment_t *seg) {
  link->seg = seg;
  pthread_mutex_lock(&link_lock);
  {
    link->prev = link;
    link->next = link;
  }
  pthread_mutex_unlock(&link_lock);
}

void link_insert(segment_t *seg, link_t *base) {
  link_t *link = (link_t *)malloc(sizeof(link_t));
  link->seg = seg;

  pthread_mutex_lock(&link_lock);
  {
    link_t *prev = base->prev;
    link->prev = prev;
    link->next = base;
    base->prev = link;
    prev->next = link;
  }
  pthread_mutex_unlock(&link_lock);
}

void link_remove(link_t *link) {
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
