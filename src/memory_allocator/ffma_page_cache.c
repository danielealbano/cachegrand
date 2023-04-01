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
#include "hugepages.h"
#include "thread.h"

#include "ffma_page_cache.h"

#define TAG "ffma_page_cache"

thread_local void* ffma_page_cache_next_mmap_addr = NULL;
static ffma_page_cache_t* page_cache = NULL;

ffma_page_cache_t* ffma_page_cache_init(
        uint64_t cache_size,
        bool use_hugepages) {
    page_cache = xalloc_alloc_zero(sizeof(ffma_page_cache_t));
    page_cache->cache_size = cache_size;
    page_cache->use_hugepages = use_hugepages;

    int numa_node_count = utils_numa_node_configured_count();

    page_cache->numa_nodes = xalloc_alloc_zero(sizeof(ffma_page_cache_t) * numa_node_count);
    for(int numa_node_index = 0; numa_node_index < numa_node_count; numa_node_index++) {
        ffma_page_cache_numa_node_t *page_cache_numa_node = &page_cache->numa_nodes[numa_node_index];
        page_cache_numa_node->free_queue = queue_mpmc_init();
    }

    return page_cache;
}

void ffma_page_cache_free() {
    int numa_node_count = utils_numa_node_configured_count();

    for(int numa_node_index = 0; numa_node_index < numa_node_count; numa_node_index++) {
        void *addr;
        queue_mpmc_t *queue = page_cache->numa_nodes[numa_node_index].free_queue;
        while((addr = queue_mpmc_pop(queue)) != NULL) {
            xalloc_mmap_free(addr, HUGEPAGE_SIZE_2MB);
        }

        queue_mpmc_free(queue);
    }

    xalloc_free(page_cache->numa_nodes);
    xalloc_free(page_cache);

    page_cache = NULL;
}

void ffma_page_cache_push(
        void* addr) {
    assert(page_cache != NULL);
    assert(addr != NULL);

    uint32_t numa_node_index = thread_get_current_numa_node_index();

    // If the queue is full free the memory
    if (queue_mpmc_get_length(page_cache->numa_nodes[numa_node_index].free_queue) >= page_cache->cache_size) {
        xalloc_mmap_free(addr, HUGEPAGE_SIZE_2MB);
    } else {
        queue_mpmc_push(page_cache->numa_nodes[numa_node_index].free_queue, addr);
    }
}

void* ffma_page_cache_pop() {
    void* addr;

    assert(page_cache != NULL);

    uint32_t numa_node_index = thread_get_current_numa_node_index();
    addr = queue_mpmc_pop(page_cache->numa_nodes[numa_node_index].free_queue);

    if (unlikely(addr == NULL)) {
        if (page_cache->use_hugepages) {
            addr = xalloc_hugepage_alloc(HUGEPAGE_SIZE_2MB);
        } else {
            // Ensure ffma_page_cache_next_mmap_addr is not null
            if (unlikely(ffma_page_cache_next_mmap_addr == NULL)) {
                // Pick a random address that is 2MB aligned
                ffma_page_cache_next_mmap_addr = xalloc_random_aligned_addr(HUGEPAGE_SIZE_2MB);
            }

            // Try to allocate it
            uint64_t retries = 0;
            xalloc_mmap_try_alloc_fixed_addr_result_t result;
            while (unlikely((result = xalloc_mmap_try_alloc_fixed_addr(
                    ffma_page_cache_next_mmap_addr,
                    HUGEPAGE_SIZE_2MB,
                    &addr)) != XALLOC_MMAP_TRY_ALLOC_FIXED_ADDR_RESULT_SUCCESS)) {
                if (unlikely(result != XALLOC_MMAP_TRY_ALLOC_FIXED_ADDR_RESULT_FAILED_ALREADY_ALLOCATED)) {
                    FATAL(TAG, "Failed to allocate memory for the ffma page cache");
                }

                retries++;

                // Log a warning every 20 retries, if it has failed to find a free address after 100 retries, give up
                if (unlikely(retries > 0 && retries % 20 == 0)) {
                    // The 64 bit address space is very large so this should never happen but if it does, it is better
                    // to hard fail after 100 retries than to loop forever.
                    if (unlikely(retries == 100)) {
                        FATAL(TAG, "Unable to find a free address for a 2MB FFMA page cache after %lu retries", retries);
                    }

                    LOG_W(TAG, "Unable to find a free address for a 2MB FFMA page cache after %lu retries\n", retries);
                }

                // Pick a new random address that is 2MB aligned
                ffma_page_cache_next_mmap_addr = xalloc_random_aligned_addr(HUGEPAGE_SIZE_2MB);
            }

            // Update the next mmap addr as the allocation was successful
            ffma_page_cache_next_mmap_addr += HUGEPAGE_SIZE_2MB;
        }
    }

    assert(addr != NULL);

    return addr;
}
