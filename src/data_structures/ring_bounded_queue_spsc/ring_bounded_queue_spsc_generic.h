/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#ifdef __cplusplus
#include <atomic>
#else
#include <stdatomic.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RING_BOUNDED_QUEUE_SPSC_ITEM_TYPE
#error "RING_BOUNDED_QUEUE_SPSC_ITEM_TYPE undefined, please define it before including ring_bounded_queue_spsc.h"
#endif

#ifndef RING_BOUNDED_QUEUE_SPSC_ITEM_NAME
#error "RING_BOUNDED_QUEUE_SPSC_ITEM_NAME undefined, please define it before including ring_bounded_queue_spsc.h"
#endif

#ifndef RING_BOUNDED_QUEUE_SPSC_ABI_EXPOSE_FOUND
#error "RING_BOUNDED_QUEUE_SPSC_ABI_EXPOSE_FOUND undefined, please define it before including ring_bounded_queue_spsc.h"
#endif

#include "misc.h"
#include "exttypes.h"
#include "pow2.h"
#include "xalloc.h"
#include "memory_fences.h"

#define RING_BOUNDED_QUEUE_SPSC_ITEM_STRUCT CONCAT(ring_bounded_queue_spsc, RING_BOUNDED_QUEUE_SPSC_ITEM_NAME)
#define RING_BOUNDED_QUEUE_SPSC_ITEM_TYPEDEF CONCAT(RING_BOUNDED_QUEUE_SPSC_ITEM_STRUCT, t)
#define RING_BOUNDED_QUEUE_SPSC_ABI(FUNCTION_NAME) CONCAT(CONCAT(ring_bounded_queue_spsc, RING_BOUNDED_QUEUE_SPSC_ITEM_NAME), FUNCTION_NAME)

typedef struct RING_BOUNDED_QUEUE_SPSC_ITEM_STRUCT RING_BOUNDED_QUEUE_SPSC_ITEM_TYPEDEF;
struct RING_BOUNDED_QUEUE_SPSC_ITEM_STRUCT {
    uint32_t size;
    uint32_t mask;
    uint32_volatile_t head;
    uint32_volatile_t tail;
    volatile RING_BOUNDED_QUEUE_SPSC_ITEM_TYPE items[];
};

static inline RING_BOUNDED_QUEUE_SPSC_ITEM_TYPEDEF *RING_BOUNDED_QUEUE_SPSC_ABI(init)(
        uint32_t size) {
    // The size of the queue should always be at least twice the count of threads, to ensure that any dequeue operation
    // will never follow straight away by an enqueue operation on the same ring slot.
    size = (uint32_t)pow2_next(size);

    RING_BOUNDED_QUEUE_SPSC_ITEM_TYPEDEF *rb = NULL;
    rb = (RING_BOUNDED_QUEUE_SPSC_ITEM_TYPEDEF *) xalloc_alloc_zero(
            sizeof(RING_BOUNDED_QUEUE_SPSC_ITEM_TYPEDEF) + (size * sizeof(RING_BOUNDED_QUEUE_SPSC_ITEM_TYPE)));

    if (!rb) {
        return NULL;
    }

    rb->size = size;
    rb->mask = size - 1;
    rb->head = 0;
    rb->tail = 0;

    return rb;
}

static inline void RING_BOUNDED_QUEUE_SPSC_ABI(free)(
        RING_BOUNDED_QUEUE_SPSC_ITEM_TYPEDEF *rb) {
    xalloc_free(rb);
}

static inline uint32_t RING_BOUNDED_QUEUE_SPSC_ABI(get_length)(
        RING_BOUNDED_QUEUE_SPSC_ITEM_TYPEDEF *rb) {
    return rb->tail - rb->head;
}

static inline bool RING_BOUNDED_QUEUE_SPSC_ABI(is_empty)(
        RING_BOUNDED_QUEUE_SPSC_ITEM_TYPEDEF *rb) {
    return RING_BOUNDED_QUEUE_SPSC_ABI(get_length)(rb) == 0;
}

static inline bool RING_BOUNDED_QUEUE_SPSC_ABI(is_full)(
        RING_BOUNDED_QUEUE_SPSC_ITEM_TYPEDEF *rb) {
    return RING_BOUNDED_QUEUE_SPSC_ABI(get_length)(rb) == rb->size;
}

static inline RING_BOUNDED_QUEUE_SPSC_ITEM_TYPE RING_BOUNDED_QUEUE_SPSC_ABI(peek)(
#if RING_BOUNDED_QUEUE_SPSC_ABI_EXPOSE_FOUND == 1
        RING_BOUNDED_QUEUE_SPSC_ITEM_TYPEDEF *rb,
        bool *found) {
#else
        RING_BOUNDED_QUEUE_SPSC_ITEM_TYPEDEF *rb) {
#endif
    MEMORY_FENCE_LOAD();

    if (unlikely(rb->head == rb->tail)) {
#if RING_BOUNDED_QUEUE_SPSC_ABI_EXPOSE_FOUND == 1
        if (likely(found)) {
            *found = false;
        }
#endif
        return 0;
    }

#if RING_BOUNDED_QUEUE_SPSC_ABI_EXPOSE_FOUND == 1
    if (likely(found)) {
        *found = true;
    }
#endif

    return (RING_BOUNDED_QUEUE_SPSC_ITEM_TYPE)rb->items[rb->head & rb->mask];
}

static inline bool RING_BOUNDED_QUEUE_SPSC_ABI(enqueue)(
        RING_BOUNDED_QUEUE_SPSC_ITEM_TYPEDEF *rb,
        RING_BOUNDED_QUEUE_SPSC_ITEM_TYPE value) {
    // First update the value, then update the tail index, all using memory fences. This will ensure that when using
    // different threads as consumer / producer, the change will be visible only after the value has been written

    MEMORY_FENCE_LOAD();

    if (unlikely(RING_BOUNDED_QUEUE_SPSC_ABI(is_full)(rb))) {
        return false;
    }

    rb->items[rb->tail & rb->mask] = value;

    MEMORY_FENCE_STORE();

    rb->tail++;

    MEMORY_FENCE_STORE();

    return true;
}

static inline RING_BOUNDED_QUEUE_SPSC_ITEM_TYPE RING_BOUNDED_QUEUE_SPSC_ABI(dequeue)(
#if RING_BOUNDED_QUEUE_SPSC_ABI_EXPOSE_FOUND == 1
        RING_BOUNDED_QUEUE_SPSC_ITEM_TYPEDEF *rb,
        bool *found) {
#else
        RING_BOUNDED_QUEUE_SPSC_ITEM_TYPEDEF *rb) {
#endif
    // First fetch the value, then update the head index, all using memory fences. This will ensure that when using
    // different threads as consumer / producer, the change will be visible only after the value has been read

    MEMORY_FENCE_LOAD();

    if (unlikely(rb->head == rb->tail)) {
#if RING_BOUNDED_QUEUE_SPSC_ABI_EXPOSE_FOUND == 1
        if (likely(found)) {
            *found = false;
        }
#endif

        return 0;
    }

#if RING_BOUNDED_QUEUE_SPSC_ABI_EXPOSE_FOUND == 1
    if (likely(found)) {
        *found = true;
    }
#endif

    RING_BOUNDED_QUEUE_SPSC_ITEM_TYPE value = (RING_BOUNDED_QUEUE_SPSC_ITEM_TYPE)rb->items[rb->head & rb->mask];

    MEMORY_FENCE_LOAD();

    rb->head++;

    MEMORY_FENCE_STORE();

    return value;
}

#ifdef __cplusplus
}
#endif
