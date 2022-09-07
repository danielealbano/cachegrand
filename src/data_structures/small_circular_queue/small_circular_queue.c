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

#include "small_circular_queue.h"

small_circular_queue_t* small_circular_queue_init(
        int16_t length) {
    // The fast fixed memory allocator doesn't have the ability to allocate more than the maximum object size therefore
    // the size of the circular queue can't be greater than the maximum size the memory allocator can allocate divided
    // by the size of the small_circular_queue struct. On 64 bit systems this should be equal to 4096.
    assert(length < (FFMA_OBJECT_SIZE_MAX / sizeof(small_circular_queue_t)));

    small_circular_queue_t *scq = NULL;
    scq = (small_circular_queue_t*)ffma_mem_alloc(sizeof(small_circular_queue_t));

    if (!scq) {
        return NULL;
    }

    scq->items = (void**)ffma_mem_alloc(length * sizeof(void*));

    if (!scq->items) {
        ffma_mem_free(scq);
        return NULL;
    }

    scq->maxsize = length;
    scq->head = 0;
    scq->tail = -1;
    scq->count = 0;

    return scq;
}

void small_circular_queue_free(
        small_circular_queue_t *scq) {
    ffma_mem_free(scq->items);
    ffma_mem_free(scq);
}

int16_t small_circular_queue_count(
        small_circular_queue_t *scq) {
    return scq->count;
}

bool small_circular_queue_is_empty(
        small_circular_queue_t *scq) {
    return !small_circular_queue_count(scq);
}

bool small_circular_queue_is_full(
        small_circular_queue_t *scq) {
    return small_circular_queue_count(scq) == scq->maxsize;
}

void *small_circular_queue_peek(
        small_circular_queue_t *scq) {
    if (small_circular_queue_is_empty(scq)) {
        return NULL;
    }

    return scq->items[scq->head];
}

bool small_circular_queue_enqueue(
        small_circular_queue_t *scq,
        void *value) {
    if (small_circular_queue_is_full(scq)) {
        return false;
    }

    scq->tail = (scq->tail + 1) % scq->maxsize;
    scq->items[scq->tail] = value;
    scq->count++;

    return true;
}

void *small_circular_queue_dequeue(
        small_circular_queue_t *scq) {
    // No need to check if scq->head == scq->tail because if it's the case scq->count will be zero and
    // small_circular_queue_is_empty will return true
    if (small_circular_queue_is_empty(scq)) {
        return NULL;
    }

    void *value = small_circular_queue_peek(scq);

    scq->head = (scq->head + 1) % scq->maxsize;
    scq->count--;

    return value;
}