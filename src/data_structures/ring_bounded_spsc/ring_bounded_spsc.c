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
#include <stdatomic.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "pow2.h"
#include "xalloc.h"

#include "ring_bounded_spsc.h"

ring_bounded_spsc_t* ring_bounded_spsc_init(
        uint32_t size) {
    // The size of the queue should always be at least twice the count of threads, to ensure that any dequeue operation
    // will never follow straight away by an enqueue operation on the same ring slot.
    size = (int16_t)pow2_next(size);

    ring_bounded_spsc_t *rb = NULL;
    rb = (ring_bounded_spsc_t*)xalloc_alloc(sizeof(ring_bounded_spsc_t));

    if (!rb) {
        return NULL;
    }

    rb->items = (volatile void**)xalloc_alloc(size * sizeof(void*));

    if (!rb->items) {
        xalloc_free(rb);
        return NULL;
    }

    rb->size = size;
    rb->mask = size - 1;
    rb->head = 0;
    rb->tail = 0;

    return rb;
}

void ring_bounded_spsc_free(
        ring_bounded_spsc_t *rb) {
    xalloc_free(rb->items);
    xalloc_free(rb);
}

uint32_t ring_bounded_spsc_get_length(
        ring_bounded_spsc_t *rb) {
    return rb->tail - rb->head;
}

bool ring_bounded_spsc_is_empty(
        ring_bounded_spsc_t *rb) {
    return ring_bounded_spsc_get_length(rb) == 0;
}

bool ring_bounded_spsc_is_full(
        ring_bounded_spsc_t *rb) {
    return ring_bounded_spsc_get_length(rb) == rb->size;
}

void *ring_bounded_spsc_peek(
        ring_bounded_spsc_t *rb) {
    MEMORY_FENCE_LOAD();

    if (unlikely(rb->head == rb->tail)) {
        return NULL;
    }

    return (void*)rb->items[rb->head & rb->mask];
}

bool ring_bounded_spsc_enqueue(
        ring_bounded_spsc_t *rb,
        void *value) {
    // First update the value, then update the tail index, all using memory fences. This will ensure that when using
    // different threads as consumer / producer, the change will be visible only after the value has been written

    MEMORY_FENCE_LOAD();

    if (unlikely(ring_bounded_spsc_is_full(rb))) {
        return false;
    }

    rb->items[rb->tail & rb->mask] = value;

    MEMORY_FENCE_STORE();

    rb->tail++;

    MEMORY_FENCE_STORE();

    return true;
}

void *ring_bounded_spsc_dequeue(
        ring_bounded_spsc_t *rb) {
    // First fetch the value, then update the head index, all using memory fences. This will ensure that when using
    // different threads as consumer / producer, the change will be visible only after the value has been read

    MEMORY_FENCE_LOAD();

    if (unlikely(rb->head == rb->tail)) {
        return NULL;
    }

    void *value = (void*)rb->items[rb->head & rb->mask];

    MEMORY_FENCE_LOAD();

    rb->head++;

    MEMORY_FENCE_STORE();

    return value;
}
