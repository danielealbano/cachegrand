/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <sched.h>
#include <assert.h>

#include "exttypes.h"
#include "misc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "xalloc.h"
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
        hugepage_cache->free_queue = queue_mpmc_init();
    }

    return hugepage_cache_per_numa_node;
}

void hugepage_cache_free() {
    int numa_node_count = utils_numa_node_configured_count();

    for(int numa_node_index = 0; numa_node_index < numa_node_count; numa_node_index++) {
        void *hugepage_addr;
        queue_mpmc_t *queue = hugepage_cache_per_numa_node[numa_node_index].free_queue;
        while((hugepage_addr = queue_mpmc_pop(queue)) != NULL) {
            xalloc_hugepage_free(hugepage_addr, HUGEPAGE_SIZE_2MB);
        }

        queue_mpmc_free(queue);
    }

    xalloc_free(hugepage_cache_per_numa_node);

    hugepage_cache_per_numa_node = NULL;
}

void hugepage_cache_push(void* hugepage_addr) {
    hugepage_cache_t* hugepage_cache;

    assert(hugepage_cache_per_numa_node != NULL);
    assert(hugepage_addr != NULL);

    uint32_t numa_node_index = thread_get_current_numa_node_index();
    hugepage_cache = &hugepage_cache_per_numa_node[numa_node_index];

    queue_mpmc_push(hugepage_cache->free_queue, hugepage_addr);
}

void* hugepage_cache_pop() {
    hugepage_cache_t* hugepage_cache;
    void* hugepage_addr;

    assert(hugepage_cache_per_numa_node != NULL);

    uint32_t numa_node_index = thread_get_current_numa_node_index();
    hugepage_cache = &hugepage_cache_per_numa_node[numa_node_index];

    hugepage_addr = queue_mpmc_pop(hugepage_cache->free_queue);

    if (unlikely(hugepage_addr == NULL)) {
        hugepage_addr = xalloc_hugepage_alloc(HUGEPAGE_SIZE_2MB);
        if (hugepage_addr == NULL) {
            return NULL;
        }
    }

    return hugepage_addr;
}
