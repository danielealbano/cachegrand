/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include "exttypes.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "utils_numa.h"
#include "hugepages.h"
#include "thread.h"
#include "xalloc.h"

#include "memory_allocator/ffma_region_cache.h"

TEST_CASE("ffma_region_cache.c", "[ffma][ffma_region_cache]") {
    SECTION("ffma_region_cache_init") {
        SECTION("no hugepages") {
            int numa_node_count = utils_numa_node_configured_count();

            ffma_region_cache_t* region_cache = ffma_region_cache_init(
                    HUGEPAGE_SIZE_2MB,
                    20,
                    false);

            REQUIRE(region_cache != nullptr);
            REQUIRE(region_cache->region_size == HUGEPAGE_SIZE_2MB);
            REQUIRE(region_cache->cache_size == 20);
            REQUIRE(region_cache->use_hugepages == false);
            REQUIRE(region_cache->numa_nodes != nullptr);

            for(int i = 0; i < numa_node_count; i++) {
                REQUIRE(region_cache->numa_nodes[i].free_queue != nullptr);
            }

            ffma_region_cache_free(region_cache);
        }

        SECTION("with hugepages") {
            ffma_region_cache_t* region_cache = ffma_region_cache_init(
                    HUGEPAGE_SIZE_2MB,
                    20,
                    true);

            REQUIRE(region_cache != nullptr);
            REQUIRE(region_cache->use_hugepages == true);

            ffma_region_cache_free(region_cache);
        }
    }

    SECTION("ffma_region_cache_pop") {
        uint32_t numa_node_index = thread_get_current_numa_node_index();

        ffma_region_cache_t* region_cache = ffma_region_cache_init(
                HUGEPAGE_SIZE_2MB,
                20,
                false);

        SECTION("pop one page from cache") {
            void* addr1 = ffma_region_cache_pop(region_cache);

            REQUIRE(queue_mpmc_get_length(region_cache->numa_nodes[numa_node_index].free_queue) ==
                0);

            xalloc_mmap_free(addr1, HUGEPAGE_SIZE_2MB);
        }

        SECTION("pop two pages from cache") {
            void* addr1 = ffma_region_cache_pop(region_cache);
            void* addr2 = ffma_region_cache_pop(region_cache);

            REQUIRE(queue_mpmc_get_length(region_cache->numa_nodes[numa_node_index].free_queue) ==
                0);

            xalloc_mmap_free(addr1, HUGEPAGE_SIZE_2MB);
            xalloc_mmap_free(addr2, HUGEPAGE_SIZE_2MB);
        }

        SECTION("pop three pages from cache") {
            void* addr1 = ffma_region_cache_pop(region_cache);
            void* addr2 = ffma_region_cache_pop(region_cache);
            void* addr3 = ffma_region_cache_pop(region_cache);

            REQUIRE(queue_mpmc_get_length(region_cache->numa_nodes[numa_node_index].free_queue) ==
                0);

            xalloc_mmap_free(addr1, HUGEPAGE_SIZE_2MB);
            xalloc_mmap_free(addr2, HUGEPAGE_SIZE_2MB);
            xalloc_mmap_free(addr3, HUGEPAGE_SIZE_2MB);
        }

        ffma_region_cache_free(region_cache);
    }

    SECTION("ffma_region_cache_push") {
        uint32_t numa_node_index = thread_get_current_numa_node_index();

        ffma_region_cache_t* region_cache = ffma_region_cache_init(
                HUGEPAGE_SIZE_2MB,
                20,
                false);

        SECTION("pop and push one page from cache") {
            void* addr = ffma_region_cache_pop(region_cache);
            ffma_region_cache_push(region_cache, addr);

            REQUIRE(queue_mpmc_get_length(region_cache->numa_nodes[numa_node_index].free_queue) ==
                1);
        }

        SECTION("pop and push two pages from cache") {
            void* addr1 = ffma_region_cache_pop(region_cache);
            void* addr2 = ffma_region_cache_pop(region_cache);
            ffma_region_cache_push(region_cache, addr1);
            ffma_region_cache_push(region_cache, addr2);

            REQUIRE(queue_mpmc_get_length(region_cache->numa_nodes[numa_node_index].free_queue) ==
                2);
        }

        SECTION("pop and push three pages from cache") {
            void* addr1 = ffma_region_cache_pop(region_cache);
            void* addr2 = ffma_region_cache_pop(region_cache);
            void* addr3 = ffma_region_cache_pop(region_cache);
            ffma_region_cache_push(region_cache, addr1);
            ffma_region_cache_push(region_cache, addr2);
            ffma_region_cache_push(region_cache, addr3);

            REQUIRE(queue_mpmc_get_length(region_cache->numa_nodes[numa_node_index].free_queue) ==
                3);
        }

        SECTION("pop three and push two pages from/to cache") {
            void* addr1 = ffma_region_cache_pop(region_cache);
            void* addr2 = ffma_region_cache_pop(region_cache);
            void* addr3 = ffma_region_cache_pop(region_cache);
            ffma_region_cache_push(region_cache, addr1);
            ffma_region_cache_push(region_cache, addr2);

            REQUIRE(region_cache->numa_nodes[numa_node_index].free_queue != nullptr);
            REQUIRE(queue_mpmc_get_length(region_cache->numa_nodes[numa_node_index].free_queue) ==
                2);

            xalloc_mmap_free(addr3, HUGEPAGE_SIZE_2MB);
        }

        SECTION("fill the cache") {
            void** addr = (void**)malloc(sizeof(void*) * (region_cache->cache_size + 1));

            // Fill the cache
            for(int i = 0; i < region_cache->cache_size + 1; i++) {
                addr[i] = ffma_region_cache_pop(region_cache);
            }
            for(int i = 0; i < region_cache->cache_size; i++) {
                ffma_region_cache_push(region_cache, addr);
            }

            // Try to push one more page
            ffma_region_cache_push(region_cache, addr[region_cache->cache_size]);

            // Ensure that the size of the cache hasn't grown
            REQUIRE(queue_mpmc_get_length(region_cache->numa_nodes[numa_node_index].free_queue) ==
                region_cache->cache_size);
        }

        ffma_region_cache_free(region_cache);
    }
}
