/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
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
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"

#include "ring_bounded_spsc.h"

ring_bounded_spsc_t* ring_bounded_spsc_init(
        int16_t length) {
    // The fast fixed memory allocator doesn't have the ability to allocate more than the maximum object size therefore
    // the size of the circular queue can't be greater than the maximum size the memory allocator can allocate divided
    // by the size of the ring_bounded_spsc struct. On 64-bit systems this should be equal to 4096.
    assert(length < (FFMA_OBJECT_SIZE_MAX / sizeof(ring_bounded_spsc_t)));

    ring_bounded_spsc_t *rb = NULL;
    rb = (ring_bounded_spsc_t*)ffma_mem_alloc(sizeof(ring_bounded_spsc_t));

    if (!rb) {
        return NULL;
    }

    rb->items = (void**)ffma_mem_alloc(length * sizeof(void*));

    if (!rb->items) {
        ffma_mem_free(rb);
        return NULL;
    }

    rb->maxsize = length;
    rb->head = 0;
    rb->tail = -1;
    rb->count = 0;

    return rb;
}

void ring_bounded_spsc_free(
        ring_bounded_spsc_t *rb) {
    ffma_mem_free(rb->items);
    ffma_mem_free(rb);
}

int16_t ring_bounded_spsc_count(
        ring_bounded_spsc_t *rb) {
    return rb->count;
}

bool ring_bounded_spsc_is_empty(
        ring_bounded_spsc_t *rb) {
    return !ring_bounded_spsc_count(rb);
}

bool ring_bounded_spsc_is_full(
        ring_bounded_spsc_t *rb) {
    return ring_bounded_spsc_count(rb) == rb->maxsize;
}

void *ring_bounded_spsc_peek(
        ring_bounded_spsc_t *rb) {
    if (ring_bounded_spsc_is_empty(rb)) {
        return NULL;
    }

    return rb->items[rb->head];
}

bool ring_bounded_spsc_enqueue(
        ring_bounded_spsc_t *rb,
        void *value) {
    if (ring_bounded_spsc_is_full(rb)) {
        return false;
    }

    rb->tail = (rb->tail + 1) % rb->maxsize;
    rb->items[rb->tail] = value;
    rb->count++;

    return true;
}

void *ring_bounded_spsc_dequeue(
        ring_bounded_spsc_t *rb) {
    // No need to check if rb->head == rb->tail because if it's the case rb->count will be zero and
    // ring_bounded_spsc_is_empty will return true
    if (ring_bounded_spsc_is_empty(rb)) {
        return NULL;
    }

    void *value = ring_bounded_spsc_peek(rb);

    rb->head = (rb->head + 1) % rb->maxsize;
    rb->count--;

    return value;
}