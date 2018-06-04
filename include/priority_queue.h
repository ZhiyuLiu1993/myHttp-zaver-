//
// Created by root on 5/22/18.
//

#ifndef MYHTTP_PRIORITY_QUEUE_H
#define MYHTTP_PRIORITY_QUEUE_H

#include "dbg.h"
#include "error.h"

#define MY_PQ_DEFAULT_SIZE 10

typedef int (*my_pq_comparator_pt)(void *pi, void *pj);

typedef struct {
    void **pq;
    size_t nalloc;
    size_t size;
    my_pq_comparator_pt comp;
} my_pq_t;

int my_pq_init(my_pq_t *my_pq, my_pq_comparator_pt comp, size_t size);
int my_pq_is_empty(my_pq_t *my_pq);
size_t my_pq_size(my_pq_t *my_pq);
void *my_pq_min(my_pq_t *my_pq);
int my_pq_delmin(my_pq_t *my_pq);
int my_pq_insert(my_pq_t *my_pq, void *item);

size_t my_pq_sink(my_pq_t *my_pq, size_t i);

#endif //MYHTTP_PRIORITY_QUEUE_H
