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

#include "exttypes.h"
#include "memory_fences.h"
#include "xalloc.h"

#include "queue_mpmc.h"

queue_mpmc_t *queue_mpmc_init() {
    // This queue is used by the slab allocator, therefore the memory can't be allocated with it
    return xalloc_alloc_zero(sizeof(queue_mpmc_t));
}

void queue_mpmc_push(
        queue_mpmc_t *queue_mpmc,
        void *data) {
    queue_mpmc_node_t *node = xalloc_alloc(sizeof(queue_mpmc_node_t));
    node->data = data;

    queue_mpmc_versioned_head_t head_expected = {
            ._packed = queue_mpmc->head._packed
    };
    queue_mpmc_versioned_head_t head_new = {
            .data = {
                    .node = node,
            },
    };

    do {
        head_new.data.length = head_expected.data.length + 1;
        head_new.data.version = head_expected.data.version + 1;
        node->next = (queue_mpmc_node_t*)head_expected.data.node;
    } while (!__atomic_compare_exchange_n(
            &queue_mpmc->head._packed,
            &head_expected._packed,
            head_new._packed,
            true,
            __ATOMIC_RELEASE,
            __ATOMIC_RELAXED));
}

void *queue_mpmc_pop(
        queue_mpmc_t *queue_mpmc) {
    queue_mpmc_versioned_head_t head_expected = {
            ._packed = queue_mpmc->head._packed
    };
    queue_mpmc_versioned_head_t head_new;

    while (head_expected.data.node != NULL) {
        head_new.data.node = head_expected.data.node->next;
        head_new.data.version = head_expected.data.version + 1;
        head_new.data.length = head_expected.data.length - 1;

        if (__atomic_compare_exchange_n(
                &queue_mpmc->head._packed,
                &head_expected._packed,
                head_new._packed,
                true,
                __ATOMIC_RELEASE,
                __ATOMIC_ACQUIRE)) {
            break;
        }
    }

    void *data = head_expected.data.node->data;

    xalloc_free((queue_mpmc_node_t*)head_expected.data.node);

    return data;
}

uint32_t queue_mpmc_get_length(
        queue_mpmc_t *queue_mpmc) {
    MEMORY_FENCE_LOAD();
    return (uint32_t)queue_mpmc->head.data.length;
}

queue_mpmc_node_t *queue_mpmc_peek(
        queue_mpmc_t *queue_mpmc) {
    MEMORY_FENCE_LOAD();
    return (queue_mpmc_node_t*)queue_mpmc->head.data.node;
}

bool queue_mpmc_is_empty(
        queue_mpmc_t *queue_mpmc) {
    return queue_mpmc_peek(queue_mpmc) == NULL;
}
