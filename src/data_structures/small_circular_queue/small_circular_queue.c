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

#include "xalloc.h"

#include "small_circular_queue.h"

small_circular_queue_t* small_circular_queue_init(
        int16_t length) {
    small_circular_queue_t *scq = NULL;
    scq = (small_circular_queue_t*)xalloc_alloc_small(sizeof(small_circular_queue_t));

    if (!scq) {
        return NULL;
    }

    scq->items = (void**)xalloc_alloc(length * sizeof(void*));

    if (!scq->items) {
        xalloc_free(scq);
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
    xalloc_free(scq->items);
    xalloc_free(scq);
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
