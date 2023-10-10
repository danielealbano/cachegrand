/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>
#include <numa.h>
#include <errno.h>

#include "exttypes.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_rwspinlock.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"

#include "fixtures-hashtable-mpmc.h"

TEST_CASE("hashtable/hashtable.c", "[hashtable][hashtable]") {
    SECTION("hashtable_mcmp_init") {
        SECTION("5 buckets, non resizable, non numa aware") {
            HASHTABLE(buckets_initial_count_5, false, {
                REQUIRE(hashtable != NULL);
                REQUIRE(hashtable->config->numa_aware == false);
                REQUIRE(hashtable->config->numa_nodes_bitmask == NULL);
            })
        }

        SECTION("5 buckets, non resizable, numa aware (all nodes)") {
            if (numa_available() == 0 && numa_num_configured_nodes() >= 2) {
                HASHTABLE_NUMA_AWARE(0x7FFF, false, numa_all_nodes_ptr, {
                    REQUIRE(hashtable != NULL);
                    REQUIRE(hashtable->config->numa_aware == true);
                    REQUIRE(hashtable->config->numa_nodes_bitmask == numa_all_nodes_ptr);
                })
            } else {
                WARN("Can't test numa awareness, numa not available or only one numa node");
            }
        }

        SECTION("5 buckets, non resizable, numa aware (only first node)") {
            if (numa_available() == 0 && numa_num_configured_nodes() >= 2) {
                struct bitmask* numa_first_node_ptr = numa_allocate_nodemask();
                numa_bitmask_setbit(numa_first_node_ptr, 0);

                HASHTABLE_NUMA_AWARE(0x7FFF, false, numa_first_node_ptr, {
                    REQUIRE(hashtable != NULL);
                    REQUIRE(hashtable->config->numa_aware == true);
                    REQUIRE(hashtable->config->numa_nodes_bitmask == numa_first_node_ptr);
                })

                numa_free_nodemask(numa_first_node_ptr);
            } else {
                WARN("Can't test numa awareness, numa not available or only one numa node");
            }
        }

        SECTION("5 buckets, non resizable, numa aware (no nodes)") {
            HASHTABLE_NUMA_AWARE(0x7FFF, false, numa_no_nodes_ptr, {
                REQUIRE(hashtable == NULL);
            })
        }
    }

    SECTION("hashtable->hashtable_data->buckets_count") {
        HASHTABLE(0x07, false, {
            REQUIRE(hashtable->ht_current->buckets_count == 0x08u);
        })

        HASHTABLE(0x7F, false, {
            REQUIRE(hashtable->ht_current->buckets_count == 0x80u);
        })

        HASHTABLE(0x1FF, false, {
            REQUIRE(hashtable->ht_current->buckets_count == 0x200u);
        })
    }

    SECTION("hashtable consts") {
        SECTION("HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT == 14") {
            REQUIRE(HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT == 14);
        }
    }

    SECTION("hashtable struct size") {
        SECTION("sizeof(hashtable_key_value_t) == 32") {
            REQUIRE(sizeof(hashtable_key_value_t) == 32);
        }

        SECTION("sizeof(hashtable_half_hashes_chunk_atomic_t) == 64") {
            REQUIRE(sizeof(hashtable_half_hashes_chunk_volatile_t) == 64);
        }

        SECTION("sizeof(hashtable_half_hashes_chunk_atomic.metadata.padding) == 4") {
            hashtable_half_hashes_chunk_volatile_t hashtable_half_hashes_chunk_atomic;
            REQUIRE(sizeof(hashtable_half_hashes_chunk_atomic.metadata) == 2);
        }

        SECTION("sizeof(hashtable_half_hashes_chunk_atomic.half_hashes) == 4 * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT)") {
            hashtable_half_hashes_chunk_volatile_t hashtable_half_hashes_chunk_atomic = { 0 };
            REQUIRE(sizeof(hashtable_half_hashes_chunk_atomic.half_hashes) == 4 * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);
        }
    }
}
