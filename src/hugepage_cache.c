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
#include <sched.h>
#include <assert.h>

#include "exttypes.h"
#include "misc.h"
#include "memory_fences.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "xalloc.h"
#include "spinlock.h"
#include "utils_numa.h"
#include "hugepages.h"
#include "thread.h"

#include "hugepage_cache.h"

/**
 * Although not pointed out in the naming convention, the hugepages in use are only 2MB hugepages, cachegrand currently
 * doesn't use and don't plan to use and/or to support 1GB hugepages.
 */

hugepage_cache_t* hugepage_cache_per_numa_node = NULL;

hugepage_cache_t* hugepage_cache_init() {
    hugepage_cache_t* hugepage_cache;
    int numa_node_count = utils_numa_node_configured_count();

    hugepage_cache_per_numa_node = xalloc_alloc_zero(sizeof(hugepage_cache_t) * numa_node_count);
    for(int numa_node_index = 0; numa_node_index < numa_node_count; numa_node_index++) {
        hugepage_cache = &hugepage_cache_per_numa_node[numa_node_index];
        hugepage_cache->numa_node_index = numa_node_index;
        hugepage_cache->free_hugepages = double_linked_list_init();
        spinlock_init(&hugepage_cache->lock);
    }

    return hugepage_cache_per_numa_node;
}

void hugepage_cache_free() {
    double_linked_list_t* list;
    double_linked_list_item_t* item;
    int numa_node_count = utils_numa_node_configured_count();

    for(int numa_node_index = 0; numa_node_index < numa_node_count; numa_node_index++) {
        list = hugepage_cache_per_numa_node[numa_node_index].free_hugepages;
        while((item = double_linked_list_pop_item(list)) != NULL) {
            xalloc_hugepage_free(item->data, HUGEPAGE_SIZE_2MB);
            double_linked_list_item_free(item);
        }

        double_linked_list_free(list);
    }

    xalloc_free(hugepage_cache_per_numa_node);

    hugepage_cache_per_numa_node = NULL;
}

void hugepage_cache_push(void* hugepage_addr) {
    hugepage_cache_t* hugepage_cache;
    double_linked_list_item_t* item;

    assert(hugepage_cache_per_numa_node != NULL);

    /**
     * At the beginning of the double linked list the "empty" list items are kept to avoid allocating and deallocating
     * continuously the memory. It's always possible to try to get the first and check if data is NULL or not, there
     * should always be one available as it's created with the invocation of hugepage_cache_pop if a new hugepage is
     * requested
     */

    uint32_t numa_node_index = thread_get_current_numa_node_index();
    hugepage_cache = &hugepage_cache_per_numa_node[numa_node_index];

    spinlock_lock(&hugepage_cache->lock, true);

    item = hugepage_cache->free_hugepages->head;
    assert(item->data == NULL);
    item->data = hugepage_addr;
    double_linked_list_move_item_to_tail(hugepage_cache->free_hugepages, item);

    hugepage_cache->stats.in_use--;

    spinlock_unlock(&hugepage_cache->lock);
}

void* hugepage_cache_pop() {
    hugepage_cache_t* hugepage_cache;
    double_linked_list_item_t* item;
    void* hugepage_addr;

    assert(hugepage_cache_per_numa_node != NULL);

    /**
     * The double linked list items used in the hugepage_cache are hold, forever, as the hugepages allocated by the
     * software are never released back to the OS for performance reason, they are always reused when on the same
     * NUMA node.
     *
     * To avoid wasting resources and being hit by memory fragmentation on the long run, the items of the double linked
     * list that are popped from the list are pushed back to the beginning to be re-used when/if the hugepages are
     * pushed back to the cache.
     *
     * As indirect result of this implementation, the count of elements in the list represent also the amount of
     * hugepages allocated
     */

    uint32_t numa_node_index = thread_get_current_numa_node_index();
    hugepage_cache = &hugepage_cache_per_numa_node[numa_node_index];

    spinlock_lock(&hugepage_cache->lock, true);

    item = hugepage_cache->free_hugepages->tail;

    if (unlikely(item == NULL || item->data == NULL)) {
        /**
         * item will be NULL only on the first invocation, in all the other cases it will always be item->data == NULL
         * when no hugepages are available.
         */
        hugepage_addr = xalloc_hugepage_alloc(HUGEPAGE_SIZE_2MB);

        item = double_linked_list_item_init();
        item->data = NULL;
        double_linked_list_unshift_item(hugepage_cache->free_hugepages, item);

        hugepage_cache->stats.total++;
    } else {
        hugepage_addr = item->data;
        item->data = NULL;
        double_linked_list_move_item_to_head(hugepage_cache->free_hugepages, item);
    }

    hugepage_cache->stats.in_use++;

    spinlock_unlock(&hugepage_cache->lock);

    return hugepage_addr;
}
