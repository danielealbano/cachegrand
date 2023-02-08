/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>
#include <numa.h>

#include <string.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "xalloc.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "fiber/fiber.h"
#include "fiber/fiber_scheduler.h"
#include "clock.h"
#include "config.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"
#include "data_structures/hashtable/mcmp/hashtable_support_index.h"
#include "data_structures/hashtable/mcmp/hashtable_op_iter.h"

#include "../../../support.h"
#include "fixtures-hashtable-mpmc.h"

TEST_CASE("hashtable/hashtable_mcmp_op_iter.c", "[hashtable][hashtable_op][hashtable_mcmp_op_iter]") {
    worker_context_t worker_context = { 0 };
    worker_context.worker_index = UINT16_MAX;
    worker_context_set(&worker_context);
    transaction_set_worker_index(worker_context.worker_index);

    SECTION("hashtable_mcmp_op_iter") {
        SECTION("hashtable empty") {
            hashtable_bucket_index_t bucket_index = 0;

            HASHTABLE(0x7FFF, false, {
                REQUIRE(!hashtable_mcmp_op_iter(hashtable, &bucket_index));
                REQUIRE(bucket_index + 1 == hashtable->ht_current->buckets_count_real);
            })
        }

        SECTION("one key") {
            hashtable_bucket_index_t bucket_index = 0;

            HASHTABLE(0x7FFF, false, {
                // Not necessary to free, the key(s) is owned by the hashtable
                char *test_key_1_copy = (char*)ffma_mem_alloc(test_key_1_len + 1);
                strcpy(test_key_1_copy, test_key_1);

                hashtable_chunk_index_t chunk_index1 = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));
                HASHTABLE_SET_KEY_EXTERNAL_BY_INDEX(
                        chunk_index1,
                        0,
                        test_key_1_hash,
                        test_key_1_copy,
                        test_key_1_len,
                        test_value_1);

                REQUIRE((uintptr_t)hashtable_mcmp_op_iter(hashtable,&bucket_index) == test_value_1);
                REQUIRE(bucket_index == HASHTABLE_TO_BUCKET_INDEX(chunk_index1, 0));

                bucket_index++;
                REQUIRE(!hashtable_mcmp_op_iter(hashtable, &bucket_index));
                REQUIRE(bucket_index + 1 == hashtable->ht_current->buckets_count_real);
            })
        }

        SECTION("single chunk multiple slots") {
            hashtable_bucket_index_t bucket_index = 0;

            HASHTABLE(0x7FFF, false, {
                test_key_same_bucket_t *test_key_same_bucket = test_support_same_hash_mod_fixtures_generate(
                        hashtable->ht_current->buckets_count,
                        test_key_same_bucket_key_prefix_external,
                        HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);

                hashtable_bucket_index_t bucket_index_base =
                        test_key_same_bucket[0].key_hash % hashtable->ht_current->buckets_count;

                for (hashtable_chunk_slot_index_t i = 0; i < HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT; i++) {
                    // Not necessary to free, the key(s) is owned by the hashtable
                    char *test_key_same_bucket_current_copy = (char *) xalloc_alloc(test_key_same_bucket[i].key_len + 1);
                    strncpy(
                            test_key_same_bucket_current_copy,
                            test_key_same_bucket[i].key,
                            test_key_same_bucket[i].key_len + 1);

                    HASHTABLE_SET_KEY_EXTERNAL_BY_INDEX(
                            HASHTABLE_TO_CHUNK_INDEX(bucket_index_base),
                            i,
                            test_key_same_bucket[i].key_hash,
                            test_key_same_bucket_current_copy,
                            test_key_same_bucket[i].key_len,
                            test_value_1 + i);
                }

                for (hashtable_chunk_slot_index_t i = 0; i < HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT; i++) {
                    REQUIRE((uintptr_t)hashtable_mcmp_op_iter(hashtable, &bucket_index) == test_value_1 + i);
                    REQUIRE(bucket_index == HASHTABLE_TO_BUCKET_INDEX(HASHTABLE_TO_CHUNK_INDEX(bucket_index_base), i));

                    bucket_index++;
                }

                REQUIRE(!hashtable_mcmp_op_iter(hashtable, &bucket_index));
                REQUIRE(bucket_index + 1 == hashtable->ht_current->buckets_count_real);
            })
        }

        SECTION("single chunk multiple slots all deleted") {
            hashtable_bucket_index_t bucket_index = 0;

            HASHTABLE(0x7FFF, false, {
                test_key_same_bucket_t *test_key_same_bucket = test_support_same_hash_mod_fixtures_generate(
                        hashtable->ht_current->buckets_count,
                        test_key_same_bucket_key_prefix_external,
                        HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);

                hashtable_bucket_index_t bucket_index_base =
                        test_key_same_bucket[0].key_hash % hashtable->ht_current->buckets_count;

                for (hashtable_chunk_slot_index_t i = 0; i < HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT; i++) {
                    // Not necessary to free, the key(s) is owned by the hashtable
                    char *test_key_same_bucket_current_copy = (char *) xalloc_alloc(test_key_same_bucket[i].key_len + 1);
                    strncpy(
                            test_key_same_bucket_current_copy,
                            test_key_same_bucket[i].key,
                            test_key_same_bucket[i].key_len + 1);

                    HASHTABLE_SET_KEY_EXTERNAL_BY_INDEX(
                            HASHTABLE_TO_CHUNK_INDEX(bucket_index_base),
                            i,
                            test_key_same_bucket[i].key_hash,
                            test_key_same_bucket_current_copy,
                            test_key_same_bucket[i].key_len,
                            test_value_1 + i);

                    HASHTABLE_KEYS_VALUES(HASHTABLE_TO_CHUNK_INDEX(bucket_index_base), i).flags =
                            HASHTABLE_KEY_VALUE_FLAG_DELETED;
                }

                REQUIRE(!hashtable_mcmp_op_iter(hashtable, &bucket_index));
                REQUIRE(bucket_index + 1 == hashtable->ht_current->buckets_count_real);
            })
        }
    }
}
