/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
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
#include "xalloc.h"

#include "queue_mpmc.h"

/**
 * The queue_mpmc is basilar data structure, to avoid relying on the system (or mimalloc) memory allocator (as it's
 * also used by FFMA) it internally uses mmap to allocate memory for the queue nodes.
 *
 * Each page allocated has a prev and next pointer to link the pages together, all the memory afterwards is used for the
 * queue nodes.
 */

static size_t queue_mpmc_os_page_size = 0;

queue_mpmc_t *queue_mpmc_init() {
    queue_mpmc_t *queue_mpmc = xalloc_mmap_alloc(sizeof(queue_mpmc_t));
    queue_mpmc_init_noalloc(queue_mpmc);
    return queue_mpmc;
}

void queue_mpmc_init_noalloc(
        queue_mpmc_t *queue_mpmc) {
    if (unlikely(queue_mpmc_os_page_size == 0)) {
        queue_mpmc_os_page_size = xalloc_get_page_size();
    }

    // Allocate the first page for the queue nodes and force the initialization setting the first byte to 0
    queue_mpmc_page_volatile_t *nodes_page = xalloc_mmap_alloc(queue_mpmc_os_page_size);
    nodes_page->prev_page = 0;

    // Initialize the base structure and return set the nodes page pointer
    queue_mpmc->head.data.nodes_page = nodes_page;
    queue_mpmc->head.data.node_index = -1;
    queue_mpmc->max_nodes_per_page =
            (int16_t)((queue_mpmc_os_page_size - sizeof(queue_mpmc_page_volatile_t)) / sizeof(nodes_page->nodes[0]));
}

void queue_mpmc_free_nodes(
        queue_mpmc_t *queue_mpmc) {
    // This function is invoked only when the queue is freed up, no operation will be carried out on it, therefore the
    // queue can be freed up using non-atomic operations, only a load fence is needed to ensure that the local cpu
    // cache is up-to-date with the changes carried out by the atomic ops if any.
    MEMORY_FENCE_LOAD();

    queue_mpmc_page_volatile_t *nodes_page;;

    // Free all the pages after the one pointed by the head if there is any
    nodes_page = queue_mpmc->head.data.nodes_page->next_page;
    while(nodes_page != NULL) {
        queue_mpmc_page_volatile_t *current_page = nodes_page;
        nodes_page = nodes_page->next_page;
        xalloc_mmap_free((void*)current_page, queue_mpmc_os_page_size);
    }

    // Free the current page and the previous ones if there is any
    nodes_page = queue_mpmc->head.data.nodes_page;
    while(nodes_page != NULL) {
        queue_mpmc_page_volatile_t *current_page = nodes_page;
        nodes_page = nodes_page->prev_page;
        xalloc_mmap_free((void*)current_page, queue_mpmc_os_page_size);
    }
}

void queue_mpmc_free(
        queue_mpmc_t *queue_mpmc) {
    queue_mpmc_free_noalloc(queue_mpmc);
    xalloc_mmap_free(queue_mpmc, sizeof(queue_mpmc_t));
}

void queue_mpmc_free_noalloc(
        queue_mpmc_t *queue_mpmc) {
    queue_mpmc_free_nodes(queue_mpmc);
}

bool queue_mpmc_push(
        queue_mpmc_t *queue_mpmc,
        void *data) {
    queue_mpmc_versioned_head_t head_expected, head_new;
    queue_mpmc_page_volatile_t *nodes_page_new = NULL, *nodes_page_to_write;
    int16_t node_index_to_write;

    head_expected._packed = queue_mpmc->head._packed;

    do {
        head_new._packed = head_expected._packed;

        // Checks if the current page is full
        if (unlikely(head_new.data.node_index == queue_mpmc->max_nodes_per_page - 1)) {
            // Check if the current page has a next page
            if (likely(head_new.data.nodes_page->next_page)) {
                // Fetch the next page and update the head to point to it
                head_new.data.nodes_page = head_new.data.nodes_page->next_page;
            } else {
                // Allocate a new page for the queue nodes and force the initialization setting the first byte to 0
                if (likely(nodes_page_new == NULL)) {
                    nodes_page_new = xalloc_mmap_alloc(queue_mpmc_os_page_size);
                }

                // Set the previous page pointer and update the head to point to the new page
                nodes_page_new->prev_page = head_expected.data.nodes_page;
                head_new.data.nodes_page = nodes_page_new;
            }

            head_new.data.node_index = -1;
        }

        head_new.data.node_index++;
        head_new.data.length = head_expected.data.length + 1;
        head_new.data.version = head_expected.data.version + 1;

        node_index_to_write = head_new.data.node_index;
        nodes_page_to_write = head_new.data.nodes_page;
    } while (unlikely(!__atomic_compare_exchange_n(
            &queue_mpmc->head._packed,
            &head_expected._packed,
            head_new._packed,
            true,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)));

    // If a new page has been allocated, update the next pointer to the previous page to point to the new page
    if (unlikely(nodes_page_to_write == nodes_page_new)) {
        nodes_page_new->prev_page->next_page = nodes_page_new;
        MEMORY_FENCE_STORE();
    }

    // Write the data to the slot in the page only if the slot it's set to 0, otherwise the slot is being used by
    // another thread and it has to wait until the slot is free.
    uintptr_t data_expected = 0;
    uintptr_t data_new = (uintptr_t)data;
    while(unlikely(!__atomic_compare_exchange_n(
            &nodes_page_to_write->nodes[node_index_to_write],
            &data_expected,
            data_new,
            false,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE))) {
        data_expected = 0;
    }

    // If there is a newly allocated page but at the end wasn't used, free it
    if (unlikely(nodes_page_new != NULL && nodes_page_to_write != nodes_page_new)) {
        xalloc_mmap_free((void*)nodes_page_new, queue_mpmc_os_page_size);
    }

    return true;
}

void *queue_mpmc_pop(
        queue_mpmc_t *queue_mpmc) {
    queue_mpmc_versioned_head_t head_expected, head_new;
    queue_mpmc_page_volatile_t *nodes_page_to_read = NULL;
    uint16_t node_index_to_read = 0;
    void *data = NULL;

    assert(queue_mpmc != NULL);

    head_expected._packed = queue_mpmc->head._packed;

    // The atomic operation will use memory fences so it's not necessary one ad-hoc just for head_expected.data.length
    // once the code is in the loop, only at the beginning
    MEMORY_FENCE_LOAD();
    while (likely(head_expected.data.length > 0)) {
        head_new._packed = queue_mpmc->head._packed;
        node_index_to_read = head_new.data.node_index;
        nodes_page_to_read = head_new.data.nodes_page;

        head_new.data.node_index--;

        // Checks if the current page is full
        if (unlikely(head_new.data.node_index == -1)) {
            if (likely(head_new.data.nodes_page->prev_page)) {
                // The nodes page has to be updated to point to the previous one and node_index has to point at the end
                // of the page.
                head_new.data.nodes_page = head_new.data.nodes_page->prev_page;
                head_new.data.node_index = (int16_volatile_t)(queue_mpmc->max_nodes_per_page - 1);

                // Wait if next_page is null for consistency, potentially another thread will try to push causing the
                // next_page to be read as null and a new page to be allocated de-facto breaking the chain and leading
                // to a memory loss
                while(unlikely(head_new.data.nodes_page->next_page == NULL)) {
                    MEMORY_FENCE_LOAD();
                }
            }
        }

        head_new.data.version = head_expected.data.version + 1;
        head_new.data.length = head_expected.data.length - 1;

        if (likely(__atomic_compare_exchange_n(
                &queue_mpmc->head._packed,
                &head_expected._packed,
                head_new._packed,
                true,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE))) {

            break;
        }

        // If the compare exchange fails, the information needed by the code outside the loop is not valid anymore and
        // has to be cleared up, potentially there will be nothing else to read and the loop will be interrupted.
        nodes_page_to_read = NULL;
    }

    assert(head_new.data.length >= 0);

    if (likely(nodes_page_to_read != NULL)) {
        // Wait for a value in the slop of the page (non-zero) and then use a CAS operation to read it and at the same
        // time set it to zero.
        uintptr_t data_expected;
        uintptr_t data_new = 0;
        do {
            // Wait for a value in the slot of the page (non-zero)
            do {
                MEMORY_FENCE_LOAD();
                data_expected = nodes_page_to_read->nodes[node_index_to_read];
            } while (likely(data_expected == 0));
        } while(unlikely(!__atomic_compare_exchange_n(
                &nodes_page_to_read->nodes[node_index_to_read],
                &data_expected,
                data_new,
                false,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE)));

        data = (void*)data_expected;
    }

    return data;
}
