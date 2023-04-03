/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sched.h>
#include <assert.h>
#include <pthread.h>

#include "exttypes.h"
#include "misc.h"
#include "random.h"
#include "log/log.h"
#include "fatal.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "xalloc.h"
#include "utils_numa.h"
#include "thread.h"

#include "ffma_region_cache.h"

#define TAG "ffma_region_cache"

thread_local void* ffma_region_cache_next_mmap_addr = NULL;

ffma_region_cache_t* ffma_region_cache_init(
        size_t region_size,
        uint64_t cache_size,
        bool use_hugepages) {
    ffma_region_cache_t* region_cache = xalloc_alloc_zero(sizeof(ffma_region_cache_t));
    region_cache->region_size = region_size;
    region_cache->cache_size = cache_size;
    region_cache->use_hugepages = use_hugepages;

    int numa_node_count = utils_numa_node_configured_count();

    region_cache->numa_nodes = xalloc_alloc_zero(sizeof(ffma_region_cache_t) * numa_node_count);
    for(int numa_node_index = 0; numa_node_index < numa_node_count; numa_node_index++) {
        ffma_region_cache_numa_node_t *region_cache_numa_node = &region_cache->numa_nodes[numa_node_index];
        region_cache_numa_node->free_queue = queue_mpmc_init();
    }

    return region_cache;
}

void ffma_region_cache_free(
        ffma_region_cache_t* region_cache) {
    int numa_node_count = utils_numa_node_configured_count();

    for(int numa_node_index = 0; numa_node_index < numa_node_count; numa_node_index++) {
        void *addr;
        queue_mpmc_t *queue = region_cache->numa_nodes[numa_node_index].free_queue;
        while((addr = queue_mpmc_pop(queue)) != NULL) {
            xalloc_mmap_free(addr, region_cache->region_size);
        }

        queue_mpmc_free(queue);
    }

    xalloc_free(region_cache->numa_nodes);
    xalloc_free(region_cache);
}

void ffma_region_cache_push(
        ffma_region_cache_t* region_cache,
        void* addr) {
    uint32_t numa_node_index = thread_get_current_numa_node_index();

    assert(addr != NULL);
    assert(region_cache != NULL);
    assert(region_cache->numa_nodes[numa_node_index].free_queue != NULL);

    // If the queue is full free the memory
    if (queue_mpmc_get_length(region_cache->numa_nodes[numa_node_index].free_queue) >= region_cache->cache_size) {
        xalloc_mmap_free(addr, region_cache->region_size);
    } else {
        queue_mpmc_push(region_cache->numa_nodes[numa_node_index].free_queue, addr);
    }
}

void* ffma_region_cache_pop(
        ffma_region_cache_t* region_cache) {
    void* addr;

    assert(region_cache != NULL);

    uint32_t numa_node_index = thread_get_current_numa_node_index();
    addr = queue_mpmc_pop(region_cache->numa_nodes[numa_node_index].free_queue);

    if (unlikely(addr == NULL)) {
        // Ensure ffma_region_cache_next_mmap_addr is not null
        if (unlikely(ffma_region_cache_next_mmap_addr == NULL)) {
            // Pick a random address that is aligned to region_size
            ffma_region_cache_next_mmap_addr = xalloc_random_aligned_addr(
                    region_cache->region_size,
                    region_cache->region_size);
        }

        // Try to allocate it
        uint64_t retries = 0;
        xalloc_mmap_try_alloc_fixed_addr_result_t result;
        while (unlikely((result = xalloc_mmap_try_alloc_fixed_addr(
                ffma_region_cache_next_mmap_addr,
                region_cache->region_size,
                region_cache->use_hugepages,
                &addr)) != XALLOC_MMAP_TRY_ALLOC_FIXED_ADDR_RESULT_SUCCESS)) {
            if (unlikely(result != XALLOC_MMAP_TRY_ALLOC_FIXED_ADDR_RESULT_FAILED_ALREADY_ALLOCATED)) {
                FATAL(TAG, "Failed to allocate memory for the FFMA region cache");
            }

            retries++;

            // Log a warning every 20 retries, if it has failed to find a free address after 100 retries, give up
            if (unlikely(retries > 0 && retries % 20 == 0)) {
                // The 64 bit address space is very large so this should never happen but if it does, it is better
                // to hard fail after 100 retries than to loop forever.
                if (unlikely(retries == 100)) {
                    FATAL(TAG, "Unable to find a free address for a FFMA region cache after %lu retries", retries);
                }

                LOG_W(TAG, "Unable to find a free address for a FFMA region cache after %lu retries\n", retries);
            }

            // Pick a new random address that is aligned to region size
            ffma_region_cache_next_mmap_addr = xalloc_random_aligned_addr(
                    region_cache->region_size,
                    region_cache->region_size);
        }

        // Update the next mmap addr as the allocation was successful
        ffma_region_cache_next_mmap_addr += region_cache->region_size;
    }

    assert(addr != NULL);

    return addr;
}
