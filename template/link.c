#include "link.h"

void link_init(link_t* link) {
    link->prev = link;
    link->next = link;
}

void link_insert(link_t* link, link_t* base) {
    link_t* prev = base->prev;
    link->prev = prev;
    link->next = base;
    base->prev = link;
    prev->next = link;
}

void link_remove(link_t* link) {
    link_t* prev = link->prev;
    link_t* next = link->next;
    prev->next = next;
    next->prev = prev;
}
