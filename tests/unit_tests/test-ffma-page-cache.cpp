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

#include "memory_allocator/ffma_page_cache.h"

TEST_CASE("ffma_page_cache.c", "[ffma][ffma_page_cache]") {
    SECTION("ffma_page_cache_init") {
        SECTION("no hugepages") {
            int numa_node_count = utils_numa_node_configured_count();

            ffma_page_cache_free();
            ffma_page_cache_t* page_cache = ffma_page_cache_init(10, false);

            REQUIRE(page_cache != nullptr);
            REQUIRE(page_cache->cache_size == 10);
            REQUIRE(page_cache->use_hugepages == false);
            REQUIRE(page_cache->numa_nodes != nullptr);

            for(int i = 0; i < numa_node_count; i++) {
                REQUIRE(page_cache->numa_nodes[i].free_queue != nullptr);
            }

            ffma_page_cache_free();

            // To ensure that the other tests aren't affected by the page cache being freed for thist test, we re-initialize
            // it to some generic settings
            ffma_page_cache_init(10, false);
        }

        SECTION("with hugepages") {
            ffma_page_cache_free();
            ffma_page_cache_t* page_cache = ffma_page_cache_init(10, true);

            REQUIRE(page_cache != nullptr);
            REQUIRE(page_cache->use_hugepages == true);

            ffma_page_cache_free();

            // To ensure that the other tests aren't affected by the page cache being freed for thist test, we re-initialize
            // it to some generic settings
            ffma_page_cache_init(10, false);
        }
    }

    SECTION("ffma_page_cache_pop") {
        uint32_t numa_node_index = thread_get_current_numa_node_index();

        ffma_page_cache_free();
        ffma_page_cache_t* page_cache = ffma_page_cache_init(10, false);

        SECTION("pop one page from cache") {
            void* addr1 = ffma_page_cache_pop();

            REQUIRE(queue_mpmc_get_length(page_cache->numa_nodes[numa_node_index].free_queue) ==
                0);
            REQUIRE(queue_mpmc_peek(page_cache->numa_nodes[numa_node_index].free_queue) ==
                nullptr);

            xalloc_mmap_free(addr1, HUGEPAGE_SIZE_2MB);
        }

        SECTION("pop two pages from cache") {
            void* addr1 = ffma_page_cache_pop();
            void* addr2 = ffma_page_cache_pop();

            REQUIRE(queue_mpmc_get_length(page_cache->numa_nodes[numa_node_index].free_queue) ==
                0);
            REQUIRE(queue_mpmc_peek(page_cache->numa_nodes[numa_node_index].free_queue) ==
                nullptr);

            xalloc_mmap_free(addr1, HUGEPAGE_SIZE_2MB);
            xalloc_mmap_free(addr2, HUGEPAGE_SIZE_2MB);
        }

        SECTION("pop three pages from cache") {
            void* addr1 = ffma_page_cache_pop();
            void* addr2 = ffma_page_cache_pop();
            void* addr3 = ffma_page_cache_pop();

            REQUIRE(queue_mpmc_get_length(page_cache->numa_nodes[numa_node_index].free_queue) ==
                0);
            REQUIRE(queue_mpmc_peek(page_cache->numa_nodes[numa_node_index].free_queue) ==
                nullptr);

            xalloc_mmap_free(addr1, HUGEPAGE_SIZE_2MB);
            xalloc_mmap_free(addr2, HUGEPAGE_SIZE_2MB);
            xalloc_mmap_free(addr3, HUGEPAGE_SIZE_2MB);
        }

        ffma_page_cache_free();

        // To ensure that the other tests aren't affected by the page cache being freed for thist test, we re-initialize
        // it to some generic settings
        ffma_page_cache_init(10, false);
    }

    SECTION("ffma_page_cache_push") {
        uint32_t numa_node_index = thread_get_current_numa_node_index();

        ffma_page_cache_free();
        ffma_page_cache_t* page_cache = ffma_page_cache_init(10, false);

        SECTION("pop and push one page from cache") {
            void* addr = ffma_page_cache_pop();
            ffma_page_cache_push(addr);

            REQUIRE(queue_mpmc_get_length(page_cache->numa_nodes[numa_node_index].free_queue) ==
                1);
            REQUIRE(queue_mpmc_peek(page_cache->numa_nodes[numa_node_index].free_queue) ==
                addr);
        }

        SECTION("pop and push two pages from cache") {
            void* addr1 = ffma_page_cache_pop();
            void* addr2 = ffma_page_cache_pop();
            ffma_page_cache_push(addr1);
            ffma_page_cache_push(addr2);

            REQUIRE(queue_mpmc_get_length(page_cache->numa_nodes[numa_node_index].free_queue) ==
                2);
            REQUIRE(queue_mpmc_peek(page_cache->numa_nodes[numa_node_index].free_queue) ==
                addr2);
        }

        SECTION("pop and push three pages from cache") {
            void* addr1 = ffma_page_cache_pop();
            void* addr2 = ffma_page_cache_pop();
            void* addr3 = ffma_page_cache_pop();
            ffma_page_cache_push(addr1);
            ffma_page_cache_push(addr2);
            ffma_page_cache_push(addr3);

            REQUIRE(queue_mpmc_get_length(page_cache->numa_nodes[numa_node_index].free_queue) ==
                3);
            REQUIRE(queue_mpmc_peek(page_cache->numa_nodes[numa_node_index].free_queue) ==
                addr3);
        }

        SECTION("pop three and push two pages from/to cache") {
            void* addr1 = ffma_page_cache_pop();
            void* addr2 = ffma_page_cache_pop();
            void* addr3 = ffma_page_cache_pop();
            ffma_page_cache_push(addr1);
            ffma_page_cache_push(addr2);

            REQUIRE(page_cache->numa_nodes[numa_node_index].free_queue != nullptr);
            REQUIRE(queue_mpmc_get_length(page_cache->numa_nodes[numa_node_index].free_queue) ==
                2);
            REQUIRE(queue_mpmc_peek(page_cache->numa_nodes[numa_node_index].free_queue) ==
                addr2);

            xalloc_mmap_free(addr3, HUGEPAGE_SIZE_2MB);
        }

        SECTION("fill the cache") {
            void** addr = (void**)malloc(sizeof(void*) * (page_cache->cache_size + 1));

            // Fill the cache
            for(int i = 0; i < page_cache->cache_size + 1; i++) {
                addr[i] = ffma_page_cache_pop();
            }
            for(int i = 0; i < page_cache->cache_size; i++) {
                ffma_page_cache_push(addr);
            }

            // Try to push one more page
            ffma_page_cache_push(addr[page_cache->cache_size]);

            // Ensure that the size of the cache hasn't grown
            REQUIRE(queue_mpmc_get_length(page_cache->numa_nodes[numa_node_index].free_queue) ==
                page_cache->cache_size);

        }

        ffma_page_cache_free();

        // To ensure that the other tests aren't affected by the page cache being freed for thist test, we re-initialize
        // it to some generic settings
        ffma_page_cache_init(10, false);
    }
}
