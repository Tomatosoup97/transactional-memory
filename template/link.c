#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "link.h"
#include "segment.h"
#include "tm.h"

pthread_mutex_t link_lock = PTHREAD_MUTEX_INITIALIZER;

void move_to_clean(struct region_s *region, segment_t *seg) {

  if (CAS(&seg->dirty, 1, 0)) {
    assert(pthread_mutex_lock(&link_lock) == 0);

    if (DEBUG)
      printf("[%p] Moving to clean\n", seg);
    link_remove(&region->dirty_seg_links, &seg->link, true);
    link_insert(&region->seg_links, seg, true);

    assert(pthread_mutex_unlock(&link_lock) == 0);
  }
}

void move_to_dirty(struct region_s *region, segment_t *seg) {

  if (CAS(&seg->dirty, 0, 1)) {
    assert(pthread_mutex_lock(&link_lock) == 0);

    if (DEBUG)
      printf("[%p] Moving to dirty\n", seg);

    link_remove(&region->seg_links, &seg->link, true);
    link_insert(&region->dirty_seg_links, seg, true);

    assert(pthread_mutex_unlock(&link_lock) == 0);
  }
}

// TODO: this lock-full approach is definitely slow

void link_insert(link_t **base, segment_t *seg, bool lock_taken) {
  if (DEBUG)
    printf("[%p] Inserting link into %p\n", seg, (void *)*base);

  link_t *link = (link_t *)malloc(sizeof(link_t));

  if (!lock_taken) {
    pthread_mutex_lock(&link_lock);
  }

  link->seg = seg;
  seg->link = link;

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
  if (!lock_taken) {
    pthread_mutex_unlock(&link_lock);
  }
  if (VERBOSE)
    printf("[%p] Inserted %p\n", seg, (void *)link);
}

void link_append(link_t **base, link_t *link) {
  if (*base == NULL) {
    *base = link;
  } else {

    link_t *base_last = (*base)->prev;
    link_t *link_first = link;
    link_t *link_last = link->prev;

    base_last->next = link_first;
    link_first->prev = base_last;
    link_last->next = (*base);
  }
}

void link_remove(link_t **base, link_t **link, bool lock_taken) {
  if (DEBUG)
    printf("[%p] Removing link %p\n", (*link)->seg, (void *)*link);

  if (!lock_taken) {
    pthread_mutex_lock(&link_lock);
  }
  {
    bool is_last = (*link)->prev == (*link);
    bool is_base = (*link) == (*base);

    link_t *prev = (*link)->prev;
    link_t *next = (*link)->next;
    prev->next = next;
    next->prev = prev;

    if (is_base) {
      *base = next;
    }

    free(*link);
    *link = NULL;

    if (is_last)
      *base = NULL;
  }
  if (!lock_taken) {
    pthread_mutex_unlock(&link_lock);
  }
}
