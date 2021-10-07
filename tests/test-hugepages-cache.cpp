#include <catch2/catch.hpp>

#include <string.h>

#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "utils_numa.h"
#include "hugepages.h"
#include "thread.h"

#include "hugepage_cache.h"

TEST_CASE("hugepage_cache.c", "[hugepage_cache]") {
    SECTION("hugepage_cache_init") {

        int numa_node_count = utils_numa_node_configured_count();

        hugepage_cache_t* hugepage_cache_per_numa_node = hugepage_cache_init();

        REQUIRE(hugepage_cache_per_numa_node != NULL);

        for(int i = 0; i < numa_node_count; i++) {
            REQUIRE(hugepage_cache_per_numa_node[i].lock.lock == SPINLOCK_UNLOCKED);
            REQUIRE(hugepage_cache_per_numa_node[i].free_hugepages != NULL);
            REQUIRE(hugepage_cache_per_numa_node[i].free_hugepages->count == 0);
            REQUIRE(hugepage_cache_per_numa_node[i].numa_node_index == i);
            REQUIRE(hugepage_cache_per_numa_node[i].stats.in_use == 0);
            REQUIRE(hugepage_cache_per_numa_node[i].stats.total == 0);
        }

        hugepage_cache_free();
    }

    SECTION("hugepage_cache_pop") {
        if (hugepages_2mb_is_available(128)) {
            uint32_t numa_node_index = thread_get_current_numa_node_index();
            hugepage_cache_t* hugepage_cache_per_numa_node = hugepage_cache_init();

            SECTION("pop one hugepage from cache") {
                void* hugepage_addr1 = hugepage_cache_pop();

                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].lock.lock == SPINLOCK_UNLOCKED);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages != NULL);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->count == 1);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->head->data == NULL);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].numa_node_index == numa_node_index);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].stats.in_use == 1);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].stats.total == 1);
            }

            SECTION("pop two hugepage from cache") {
                void* hugepage_addr1 = hugepage_cache_pop();
                void* hugepage_addr2 = hugepage_cache_pop();

                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].lock.lock == SPINLOCK_UNLOCKED);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages != NULL);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->count == 2);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->head->data == NULL);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].numa_node_index == numa_node_index);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].stats.in_use == 2);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].stats.total == 2);
            }

            SECTION("pop three hugepage from cache") {
                void* hugepage_addr1 = hugepage_cache_pop();
                void* hugepage_addr2 = hugepage_cache_pop();
                void* hugepage_addr3 = hugepage_cache_pop();

                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].lock.lock == SPINLOCK_UNLOCKED);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages != NULL);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->count == 3);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->head->data == NULL);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].numa_node_index == numa_node_index);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].stats.in_use == 3);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].stats.total == 3);
            }

            hugepage_cache_free();
        } else {
            WARN("Can't test hugepages support in slab allocator, hugepages not enabled or not enough hugepages for testing");
        }
    }

    SECTION("hugepage_cache_push") {
        if (hugepages_2mb_is_available(128)) {
            uint32_t numa_node_index = thread_get_current_numa_node_index();
            hugepage_cache_t* hugepage_cache_per_numa_node = hugepage_cache_init();

            SECTION("pop and push one hugepage from cache") {
                void* hugepage_addr = hugepage_cache_pop();
                hugepage_cache_push(hugepage_addr);

                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].lock.lock == SPINLOCK_UNLOCKED);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages != NULL);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->count == 1);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->head->data == hugepage_addr);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->tail->data == hugepage_addr);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].numa_node_index == numa_node_index);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].stats.in_use == 0);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].stats.total == 1);
            }

            SECTION("pop and push two hugepage from cache") {
                void* hugepage_addr1 = hugepage_cache_pop();
                void* hugepage_addr2 = hugepage_cache_pop();
                hugepage_cache_push(hugepage_addr1);
                hugepage_cache_push(hugepage_addr2);

                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].lock.lock == SPINLOCK_UNLOCKED);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages != NULL);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->count == 2);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->head->data == hugepage_addr1);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->tail->data == hugepage_addr2);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].numa_node_index == numa_node_index);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].stats.in_use == 0);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].stats.total == 2);
            }

            SECTION("pop and push three hugepage from cache") {
                void* hugepage_addr1 = hugepage_cache_pop();
                void* hugepage_addr2 = hugepage_cache_pop();
                void* hugepage_addr3 = hugepage_cache_pop();
                hugepage_cache_push(hugepage_addr1);
                hugepage_cache_push(hugepage_addr2);
                hugepage_cache_push(hugepage_addr3);

                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].lock.lock == SPINLOCK_UNLOCKED);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages != NULL);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->count == 3);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->head->data == hugepage_addr1);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->tail->data == hugepage_addr3);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].numa_node_index == numa_node_index);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].stats.in_use == 0);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].stats.total == 3);
            }

            SECTION("pop three and push two hugepage from/to cache") {
                void* hugepage_addr1 = hugepage_cache_pop();
                void* hugepage_addr2 = hugepage_cache_pop();
                void* hugepage_addr3 = hugepage_cache_pop();
                hugepage_cache_push(hugepage_addr1);
                hugepage_cache_push(hugepage_addr2);

                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].lock.lock == SPINLOCK_UNLOCKED);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages != NULL);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->count == 3);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->head->data == NULL);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].free_hugepages->tail->data == hugepage_addr2);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].numa_node_index == numa_node_index);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].stats.in_use == 1);
                REQUIRE(hugepage_cache_per_numa_node[numa_node_index].stats.total == 3);
            }

            hugepage_cache_free();
        } else {
            WARN("Can't test hugepages support in slab allocator, hugepages not enabled or not enough hugepages for testing");
        }
    }
}
