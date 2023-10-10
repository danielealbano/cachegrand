/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>
#include <numa.h>

#include "exttypes.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_rwspinlock.h"
#include "fiber/fiber.h"
#include "fiber/fiber_scheduler.h"
#include "clock.h"
#include "config.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_support_index.h"

#include "fixtures-hashtable-mpmc.h"

#if CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_T1HA2 == 1
hashtable_bucket_index_t test_key_1_hash_buckets_0x80 = 100;
hashtable_bucket_index_t test_key_1_hash_buckets_0x8000 = 1508;
hashtable_bucket_index_t test_key_1_hash_buckets_0x80000000 = 6751716;
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_XXH3 == 1
hashtable_bucket_index_t test_key_1_hash_buckets_0x80 = 106;
hashtable_bucket_index_t test_key_1_hash_buckets_0x8000 = 28010;
hashtable_bucket_index_t test_key_1_hash_buckets_0x80000000 = 1109354;
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_CRC32C == 1
hashtable_bucket_index_t test_key_1_hash_buckets_0x80 = 0;
hashtable_bucket_index_t test_key_1_hash_buckets_0x8000 = 0;
hashtable_bucket_index_t test_key_1_hash_buckets_0x80000000 = 0;
#else
#error "Unsupported hash algorithm"
#endif

TEST_CASE("hashtable/hashtable_support_index.c", "[hashtable][hashtable_support][hashtable_support_index]") {
    worker_context_t worker_context = { 0 };
    worker_context.worker_index = UINT16_MAX;
    worker_context_set(&worker_context);
    transaction_set_worker_index(worker_context.worker_index);

    SECTION("hashtable_mcmp_support_index_from_hash") {
        SECTION("buckets_count - 0x80u") {
            REQUIRE(hashtable_mcmp_support_index_from_hash(
                    0x80u,
                    test_key_1_hash) == test_key_1_hash_buckets_0x80);
        }

        SECTION("buckets_count - 0x8000u") {
            REQUIRE(hashtable_mcmp_support_index_from_hash(
                    0x8000u,
                    test_key_1_hash) == test_key_1_hash_buckets_0x8000);
        }

        SECTION("buckets_count - 0x800000u") {
            REQUIRE(hashtable_mcmp_support_index_from_hash(
                    0x800000u,
                    test_key_1_hash) == test_key_1_hash_buckets_0x80000000);
        }
    }
}
