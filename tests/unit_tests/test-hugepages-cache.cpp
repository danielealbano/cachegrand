/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include "exttypes.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "utils_numa.h"
#include "hugepages.h"
#include "thread.h"
#include "xalloc.h"

#include "hugepage_cache.h"

TEST_CASE("hugepage_cache.c", "[hugepage_cache]") {
    if (hugepages_2mb_is_available(128)) {
        SECTION("hugepage_cache_init") {
            int numa_node_count = utils_numa_node_configured_count();

            hugepage_cache_t* hugepage_cache_per_numa_node = hugepage_cache_init();

            REQUIRE(hugepage_cache_per_numa_node != nullptr);

            for(int i = 0; i < numa_node_count; i++) {
                REQUIRE(hugepage_cache_per_numa_node[i].free_queue != nullptr);
                REQUIRE(hugepage_cache_per_numa_node[i].numa_node_index == i);
            }

            hugepage_cache_free();
        }

        SECTION("hugepage_cache_pop") {
            uint32_t numa_node_index = thread_get_current_numa_node_index();
            hugepage_cache_t* hugepage_cache_per_numa_node = hugepage_cache_init();

            SECTION("pop one hugepage from cache") {
                void* hugepage_addr1 = hugepage_cache_pop();

                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_queue != nullptr);
                REQUIRE(queue_mpmc_get_length(hugepage_cache_per_numa_node[numa_node_index].free_queue) ==
                    0);
                REQUIRE(queue_mpmc_peek(hugepage_cache_per_numa_node[numa_node_index].free_queue) ==
                    nullptr);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].numa_node_index == numa_node_index);

                xalloc_hugepage_free(hugepage_addr1, HUGEPAGE_SIZE_2MB);
            }

            SECTION("pop two hugepage from cache") {
                void* hugepage_addr1 = hugepage_cache_pop();
                void* hugepage_addr2 = hugepage_cache_pop();

                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_queue != nullptr);
                REQUIRE(queue_mpmc_get_length(hugepage_cache_per_numa_node[numa_node_index].free_queue) ==
                    0);
                REQUIRE(queue_mpmc_peek(hugepage_cache_per_numa_node[numa_node_index].free_queue) ==
                    nullptr);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].numa_node_index == numa_node_index);

                xalloc_hugepage_free(hugepage_addr1, HUGEPAGE_SIZE_2MB);
                xalloc_hugepage_free(hugepage_addr2, HUGEPAGE_SIZE_2MB);
            }

            SECTION("pop three hugepage from cache") {
                void* hugepage_addr1 = hugepage_cache_pop();
                void* hugepage_addr2 = hugepage_cache_pop();
                void* hugepage_addr3 = hugepage_cache_pop();

                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_queue != nullptr);
                REQUIRE(queue_mpmc_get_length(hugepage_cache_per_numa_node[numa_node_index].free_queue) ==
                    0);
                REQUIRE(queue_mpmc_peek(hugepage_cache_per_numa_node[numa_node_index].free_queue) ==
                    nullptr);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].numa_node_index == numa_node_index);

                xalloc_hugepage_free(hugepage_addr1, HUGEPAGE_SIZE_2MB);
                xalloc_hugepage_free(hugepage_addr2, HUGEPAGE_SIZE_2MB);
                xalloc_hugepage_free(hugepage_addr3, HUGEPAGE_SIZE_2MB);
            }

            hugepage_cache_free();
        }

        SECTION("hugepage_cache_push") {
            uint32_t numa_node_index = thread_get_current_numa_node_index();
            hugepage_cache_t* hugepage_cache_per_numa_node = hugepage_cache_init();

            SECTION("pop and push one hugepage from cache") {
                void* hugepage_addr = hugepage_cache_pop();
                hugepage_cache_push(hugepage_addr);

                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_queue != nullptr);
                REQUIRE(queue_mpmc_get_length(hugepage_cache_per_numa_node[numa_node_index].free_queue) ==
                    1);
                REQUIRE(queue_mpmc_peek(hugepage_cache_per_numa_node[numa_node_index].free_queue) ==
                    hugepage_addr);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].numa_node_index == numa_node_index);
            }

            SECTION("pop and push two hugepage from cache") {
                void* hugepage_addr1 = hugepage_cache_pop();
                void* hugepage_addr2 = hugepage_cache_pop();
                hugepage_cache_push(hugepage_addr1);
                hugepage_cache_push(hugepage_addr2);

                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_queue != nullptr);
                REQUIRE(queue_mpmc_get_length(hugepage_cache_per_numa_node[numa_node_index].free_queue) ==
                    2);
                REQUIRE(queue_mpmc_peek(hugepage_cache_per_numa_node[numa_node_index].free_queue) ==
                    hugepage_addr2);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].numa_node_index == numa_node_index);
            }

            SECTION("pop and push three hugepage from cache") {
                void* hugepage_addr1 = hugepage_cache_pop();
                void* hugepage_addr2 = hugepage_cache_pop();
                void* hugepage_addr3 = hugepage_cache_pop();
                hugepage_cache_push(hugepage_addr1);
                hugepage_cache_push(hugepage_addr2);
                hugepage_cache_push(hugepage_addr3);

                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_queue != nullptr);
                REQUIRE(queue_mpmc_get_length(hugepage_cache_per_numa_node[numa_node_index].free_queue) ==
                    3);
                REQUIRE(queue_mpmc_peek(hugepage_cache_per_numa_node[numa_node_index].free_queue) ==
                    hugepage_addr3);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].numa_node_index == numa_node_index);
            }

            SECTION("pop three and push two hugepage from/to cache") {
                void* hugepage_addr1 = hugepage_cache_pop();
                void* hugepage_addr2 = hugepage_cache_pop();
                void* hugepage_addr3 = hugepage_cache_pop();
                hugepage_cache_push(hugepage_addr1);
                hugepage_cache_push(hugepage_addr2);

                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_queue != nullptr);
                REQUIRE(queue_mpmc_get_length(hugepage_cache_per_numa_node[numa_node_index].free_queue) ==
                    2);
                REQUIRE(queue_mpmc_peek(hugepage_cache_per_numa_node[numa_node_index].free_queue) ==
                    hugepage_addr2);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].numa_node_index == numa_node_index);

                xalloc_hugepage_free(hugepage_addr3, HUGEPAGE_SIZE_2MB);
            }

            hugepage_cache_free();
        }
    } else {
        WARN("Can't test fast fixed memory allocator, hugepages not enabled or not enough hugepages for testing, at least 128 2mb hugepages are required");
    }
}
