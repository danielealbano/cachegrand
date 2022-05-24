/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "slab_allocator.h"

#include "small_circular_queue.h"

small_circular_queue_t* small_circular_queue_init(
        int16_t length) {
    // The slab allocator doesn't have the ability to allocate more than the maximum object size therefore the size
    // of the circular queue can't be greater than the maximum size the slab allocator can allocate divided by the
    // size of the circular_queue struct. On 64 bit systems this should be equal to 4096
    assert(length < (SLAB_OBJECT_SIZE_MAX / sizeof(small_circular_queue_t)));

    small_circular_queue_t *cq = NULL;
    cq = (small_circular_queue_t*)slab_allocator_mem_alloc(sizeof(small_circular_queue_t));

    if (!cq) {
        return NULL;
    }

    cq->items = (void**)slab_allocator_mem_alloc(length * sizeof(void*));

    if (!cq->items) {
        slab_allocator_mem_free(cq);
        return NULL;
    }

    cq->maxsize = length;
    cq->head = 0;
    cq->tail = -1;
    cq->count = 0;

    return cq;
}

void small_circular_queue_free(
        small_circular_queue_t *cq) {
    slab_allocator_mem_free(cq->items);
    slab_allocator_mem_free(cq);
}

int16_t small_circular_queue_count(
        small_circular_queue_t *cq) {
    return cq->count;
}

bool small_circular_queue_is_empty(
        small_circular_queue_t *cq) {
    return !small_circular_queue_count(cq);
}

bool small_circular_queue_is_full(
        small_circular_queue_t *cq) {
    return small_circular_queue_count(cq) == cq->maxsize;
}

void *small_circular_queue_peek(
        small_circular_queue_t *cq) {
    if (small_circular_queue_is_empty(cq)) {
        return NULL;
    }

    return cq->items[cq->head];
}

bool small_circular_queue_enqueue(
        small_circular_queue_t *cq,
        void *value) {
    if (small_circular_queue_is_full(cq)) {
        return false;
    }

    cq->tail = (cq->tail + 1) % cq->maxsize;
    cq->items[cq->tail] = value;
    cq->count++;

    return true;
}

void *small_circular_queue_dequeue(
        small_circular_queue_t *cq) {
    // No need to check if cq->head == cq->tail because if it's the case cq->count will be zero and
    // small_circular_queue_is_empty will return true
    if (small_circular_queue_is_empty(cq)) {
        return NULL;
    }

    void *value = small_circular_queue_peek(cq);

    cq->head = (cq->head + 1) % cq->maxsize;
    cq->count--;

    return value;
}
