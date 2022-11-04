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

#include "misc.h"
#include "exttypes.h"
#include "pow2.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"

#include "ring_bounded_mpmc.h"

ring_bounded_mpmc_t* ring_bounded_mpmc_init(
        int16_t size) {
    size = (int16_t)pow2_next(size);

    // The fast fixed memory allocator doesn't have the ability to allocate more than the maximum object size therefore
    // the size of the circular queue can't be greater than the maximum size the memory allocator can allocate divided
    // by the size of the ring_bounded_mpmc struct. On 64-bit systems this should be equal to 4096.
    assert(size <= (FFMA_OBJECT_SIZE_MAX / sizeof(void*)));

    ring_bounded_mpmc_t *rb = NULL;
    rb = (ring_bounded_mpmc_t*)ffma_mem_alloc(sizeof(ring_bounded_mpmc_t));

    if (!rb) {
        return NULL;
    }

    rb->items = (void**)ffma_mem_alloc_zero(size * sizeof(void*));

    if (!rb->items) {
        ffma_mem_free(rb);
        return NULL;
    }

    rb->size = size;
    rb->mask = (int16_t)(size - 1);
    rb->head = 0;
    rb->tail = 0;

    return rb;
}

void ring_bounded_mpmc_free(
        ring_bounded_mpmc_t *rb) {
    ffma_mem_free(rb->items);
    ffma_mem_free(rb);
}

bool ring_bounded_mpmc_enqueue(
        ring_bounded_mpmc_t *rb,
        void *value) {
    uint64_t tail = __atomic_load_n(&rb->tail, __ATOMIC_ACQUIRE);

    do {
        void *current_value = __atomic_load_n(&rb->items[tail & rb->mask], __ATOMIC_ACQUIRE);
        if (unlikely(current_value != NULL)) {
            return false;
        }
    } while (!__atomic_compare_exchange_n(
            &rb->tail,
            &tail,
            tail + 1,
            true,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE));

    __atomic_store_n(&rb->items[tail & rb->mask], value, __ATOMIC_RELEASE);

    return true;
}

void *ring_bounded_mpmc_dequeue(
        ring_bounded_mpmc_t *rb) {
    void *value = NULL;
    uint64_t head = __atomic_load_n(&rb->head, __ATOMIC_ACQUIRE);

    do {
        value = __atomic_load_n(&rb->items[head & rb->mask], __ATOMIC_ACQUIRE);
        if (unlikely(value == NULL)) {
            return NULL;
        }
    } while (!__atomic_compare_exchange_n(
            &rb->head,
            &head,
            head + 1,
            true,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE));

     __atomic_store_n(&rb->items[head & rb->mask], NULL, __ATOMIC_RELEASE);

    return value;
}
