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
#include <stdatomic.h>

#include "exttypes.h"
#include "memory_fences.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"

#include "ring_bounded_mpmc.h"

ring_bounded_mpmc_t* ring_bounded_mpmc_init(
        int16_t length) {
    // The fast fixed memory allocator doesn't have the ability to allocate more than the maximum object size therefore
    // the size of the circular queue can't be greater than the maximum size the memory allocator can allocate divided
    // by the size of the ring_bounded_mpmc struct. On 64-bit systems this should be equal to 4096.
    assert(length < (FFMA_OBJECT_SIZE_MAX / sizeof(ring_bounded_mpmc_t)));

    ring_bounded_mpmc_t *rb = NULL;
    rb = (ring_bounded_mpmc_t*)ffma_mem_alloc(sizeof(ring_bounded_mpmc_t));

    if (!rb) {
        return NULL;
    }

    rb->items = (void**)ffma_mem_alloc(length * sizeof(void*));

    if (!rb->items) {
        ffma_mem_free(rb);
        return NULL;
    }

    rb->header.data.maxsize = length;
    rb->header.data.head = 0;
    rb->header.data.tail = -1;
    rb->header.data.count = 0;

    return rb;
}

void ring_bounded_mpmc_free(
        ring_bounded_mpmc_t *rb) {
    ffma_mem_free(rb->items);
    ffma_mem_free(rb);
}

int16_t ring_bounded_mpmc_count(
        ring_bounded_mpmc_t *rb) {
    MEMORY_FENCE_LOAD();
    return rb->header.data.count;
}

bool ring_bounded_mpmc_is_empty(
        ring_bounded_mpmc_t *rb) {
    return !ring_bounded_mpmc_count(rb);
}

bool ring_bounded_mpmc_is_full(
        ring_bounded_mpmc_t *rb) {
    // No need for memory fences, they will be issued by ring_bounded_mpmc_count
    return ring_bounded_mpmc_count(rb) == rb->header.data.maxsize;
}

void *ring_bounded_mpmc_peek(
        ring_bounded_mpmc_t *rb) {
    if (ring_bounded_mpmc_is_empty(rb)) {
        return NULL;
    }

    // No need for memory fences, they will be issued by ring_bounded_mpmc_count
    return rb->items[rb->header.data.head];
}

bool ring_bounded_mpmc_enqueue(
        ring_bounded_mpmc_t *rb,
        void *value) {
    if (ring_bounded_mpmc_is_full(rb)) {
        return false;
    }

    ring_bounded_mpmc_header_t header_expected;
    ring_bounded_mpmc_header_t header_new;
    do {
        header_expected._id = rb->header._id;
        header_new._id = header_expected._id;

        header_new.data.tail = (header_new.data.tail + 1) % (int16_t)header_new.data.maxsize;
        header_new.data.count++;
    } while (!__atomic_compare_exchange_n(
            &rb->header._id,
            &header_expected._id,
            header_new._id,
            true,
            __ATOMIC_ACQ_REL,
            __ATOMIC_RELAXED));

    rb->items[header_new.data.tail] = value;

    return true;
}

void *ring_bounded_mpmc_dequeue(
        ring_bounded_mpmc_t *rb) {
    // No need to check if rb->header.data.head == rb->header.data.tail because if it's the case rb->header.data.count will be zero and
    // ring_bounded_mpmc_is_empty will return true
    if (ring_bounded_mpmc_is_empty(rb)) {
        return NULL;
    }

    ring_bounded_mpmc_header_t header_expected;
    ring_bounded_mpmc_header_t header_new;
    do {
        header_expected._id = rb->header._id;
        header_new._id = header_expected._id;

        header_new.data.head = (header_new.data.head + 1) % (int16_t)header_new.data.maxsize;
        header_new.data.count--;
    } while (!__atomic_compare_exchange_n(
            &rb->header._id,
            &header_expected._id,
            header_new._id,
            true,
            __ATOMIC_ACQ_REL,
            __ATOMIC_RELAXED));

    return rb->items[header_expected.data.head];
}
