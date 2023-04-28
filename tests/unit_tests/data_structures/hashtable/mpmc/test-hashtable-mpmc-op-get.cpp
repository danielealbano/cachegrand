/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>
#include <cmath>
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
#include "data_structures/hashtable/mcmp/hashtable_op_get.h"

#include "../../../support.h"
#include "fixtures-hashtable-mpmc.h"

TEST_CASE("hashtable/hashtable_mcmp_op_get.c", "[hashtable][hashtable_op][hashtable_mcmp_op_get]") {
    worker_context_t worker_context = { 0 };
    worker_context.worker_index = UINT16_MAX;
    worker_context_set(&worker_context);
    transaction_set_worker_index(worker_context.worker_index);

    SECTION("hashtable_mcmp_op_get") {
        hashtable_value_data_t value = 0;

        SECTION("not found - hashtable empty") {
            HASHTABLE(0x7FFF, false, {
                REQUIRE(!hashtable_mcmp_op_get(
                        hashtable,
                        0,
                        test_key_1,
                        test_key_1_len,
                        &value));
            })
        }

        SECTION("found - key external") {
            HASHTABLE(0x7FFF, false, {
                // Not necessary to free, the key is owned by the hashtable
                char *test_key_1_copy = (char*)xalloc_alloc(test_key_1_len + 1);
                strcpy(test_key_1_copy, test_key_1);

                hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));

                HASHTABLE_SET_KEY_DB_0_BY_INDEX(
                        chunk_index,
                        0,
                        test_key_1_hash,
                        test_key_1_copy,
                        test_key_1_len,
                        test_value_1);

                REQUIRE(hashtable_mcmp_op_get(
                        hashtable,
                        0,
                        test_key_1_copy,
                        test_key_1_len,
                        &value));

                REQUIRE(value == test_value_1);
            })
        }

        SECTION("found - multiple chunks first slot") {
            HASHTABLE(0x7FFF, false, {
                // Not necessary to free, the key(s) is owned by the hashtable
                char *test_key_1_copy = (char*)xalloc_alloc(test_key_1_len + 1);
                char *test_key_2_copy = (char*)xalloc_alloc(test_key_1_len + 1);
                strcpy(test_key_1_copy, test_key_1);
                strcpy(test_key_2_copy, test_key_2);

                hashtable_chunk_index_t chunk_index1 = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));
                hashtable_chunk_index_t chunk_index2 = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_2_hash));

                HASHTABLE_SET_KEY_DB_0_BY_INDEX(
                        chunk_index1,
                        0,
                        test_key_1_hash,
                        test_key_1_copy,
                        test_key_1_len,
                        test_value_1);

                HASHTABLE_SET_KEY_DB_0_BY_INDEX(
                        chunk_index2,
                        0,
                        test_key_2_hash,
                        test_key_2_copy,
                        test_key_2_len,
                        test_value_2);

                REQUIRE(hashtable_mcmp_op_get(
                        hashtable,
                        0,
                        test_key_1_copy,
                        test_key_1_len,
                        &value));

                REQUIRE(value == test_value_1);

                REQUIRE(hashtable_mcmp_op_get(
                        hashtable,
                        0,
                        test_key_2_copy,
                        test_key_2_len,
                        &value));

                REQUIRE(value == test_value_2);
            })
        }

        SECTION("found - single chunk with first slot empty") {
            HASHTABLE(0x7FFF, false, {
                // Not necessary to free, the key(s) is owned by the hashtable
                char *test_key_1_copy = (char*)xalloc_alloc(test_key_1_len + 1);
                strcpy(test_key_1_copy, test_key_1);

                hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));

                HASHTABLE_SET_KEY_DB_0_BY_INDEX(
                        chunk_index,
                        1,
                        test_key_1_hash,
                        test_key_1_copy,
                        test_key_1_len,
                        test_value_1);

                REQUIRE(hashtable_mcmp_op_get(
                        hashtable,
                        0,
                        test_key_1_copy,
                        test_key_1_len,
                        &value));

                REQUIRE(value == test_value_1);
            })
        }

        SECTION("found - single chunk multiple slots - key prefix/external") {
            HASHTABLE(0x7FFF, false, {
                test_key_same_bucket_t* test_key_same_bucket = test_support_same_hash_mod_fixtures_generate(
                        0,
                        hashtable->ht_current->buckets_count,
                        test_key_same_bucket_key_prefix_external,
                        HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);

                hashtable_bucket_index_t bucket_index_base =
                        test_key_same_bucket[0].key_hash % hashtable->ht_current->buckets_count;

                for(hashtable_chunk_slot_index_t i = 0; i < HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT; i++) {
                    // Not necessary to free, the key(s) is owned by the hashtable
                    char *test_key_same_bucket_current_copy = (char*)xalloc_alloc(test_key_same_bucket[i].key_len + 1);
                    strncpy(
                            test_key_same_bucket_current_copy,
                            test_key_same_bucket[i].key,
                            test_key_same_bucket[i].key_len + 1);

                    HASHTABLE_SET_KEY_DB_0_BY_INDEX(
                            HASHTABLE_TO_CHUNK_INDEX(bucket_index_base),
                            i,
                            test_key_same_bucket[i].key_hash,
                            test_key_same_bucket_current_copy,
                            test_key_same_bucket[i].key_len,
                            test_value_1 + i);
                }

                for(hashtable_chunk_slot_index_t i = 0; i < HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT; i++) {
                    REQUIRE(hashtable_mcmp_op_get(
                            hashtable,
                            0,
                            (char *) test_key_same_bucket[i].key,
                            test_key_same_bucket[i].key_len,
                            &value));
                    REQUIRE(value == test_value_1 + i);
                }

                test_support_same_hash_mod_fixtures_free(test_key_same_bucket);
            })
        }

        SECTION("found - multiple chunks multiple slots - key prefix/external") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_count_t chunks_to_set = 3;
                hashtable_chunk_slot_index_t slots_to_fill =
                        (HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT * chunks_to_set) + 3;
                test_key_same_bucket_t* test_key_same_bucket = test_support_same_hash_mod_fixtures_generate(
                        0,
                        hashtable->ht_current->buckets_count,
                        test_key_same_bucket_key_prefix_external,
                        slots_to_fill);

                hashtable_bucket_index_t bucket_index_base =
                        test_key_same_bucket[0].key_hash % hashtable->ht_current->buckets_count;
                hashtable_chunk_index_t chunk_index_base = HASHTABLE_TO_CHUNK_INDEX(bucket_index_base);

                hashtable->ht_current->half_hashes_chunk[chunk_index_base].metadata.overflowed_chunks_counter =
                        ceil((double)slots_to_fill / HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);

                for(hashtable_chunk_slot_index_t i = 0; i < slots_to_fill; i++) {
                    // Not necessary to free, the key(s) is owned by the hashtable
                    char *test_key_same_bucket_current_copy = (char*)xalloc_alloc(test_key_same_bucket[i].key_len + 1);
                    strncpy(
                            test_key_same_bucket_current_copy,
                            test_key_same_bucket[i].key,
                            test_key_same_bucket[i].key_len + 1);

                    hashtable_chunk_index_t chunk_index =
                            chunk_index_base + (int)(i / HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);
                    hashtable_chunk_slot_index_t chunk_slot_index =
                            i % HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;

                    HASHTABLE_SET_KEY_DB_0_BY_INDEX(
                            chunk_index,
                            chunk_slot_index,
                            test_key_same_bucket[i].key_hash,
                            test_key_same_bucket_current_copy,
                            test_key_same_bucket[i].key_len,
                            test_value_1 + i);
                    HASHTABLE_HALF_HASHES_CHUNK(chunk_index).half_hashes[chunk_slot_index].distance =
                            chunk_index - chunk_index_base;
                }

                for(hashtable_chunk_slot_index_t i = 0; i < slots_to_fill; i++) {
                    hashtable_chunk_index_t chunk_index =
                            chunk_index_base + (int)(i / HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);
                    hashtable_chunk_slot_index_t chunk_slot_index =
                            i % HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;

                    REQUIRE(hashtable_mcmp_op_get(
                            hashtable,
                            0,
                            (char *) test_key_same_bucket[i].key,
                            test_key_same_bucket[i].key_len,
                            &value));
                    REQUIRE(value == test_value_1 + i);
                }

                test_support_same_hash_mod_fixtures_free(test_key_same_bucket);
            })
        }

        SECTION("not found - deleted flag") {
            HASHTABLE(0x7FFF, false, {
                // Not necessary to free, the key(s) is owned by the hashtable
                char *test_key_1_copy = (char*)xalloc_alloc(test_key_1_len + 1);
                strcpy(test_key_1_copy, test_key_1);

                HASHTABLE_SET_KEY_DB_0_BY_INDEX(
                        HASHTABLE_TO_CHUNK_INDEX(test_key_1_hash & (0x8000 - 1)),
                        0,
                        test_key_1_hash,
                        test_key_1_copy,
                        test_key_1_len,
                        test_value_1);

                HASHTABLE_KEYS_VALUES(HASHTABLE_TO_CHUNK_INDEX(test_key_1_hash & (0x8000 - 1)), 0).flags =
                        HASHTABLE_KEY_VALUE_FLAG_DELETED;

                REQUIRE(!hashtable_mcmp_op_get(
                        hashtable,
                        0,
                        test_key_1,
                        test_key_1_len,
                        &value));
            })
        }

        SECTION("not found - hash set but key_value not (edge case because of parallelism)") {
            HASHTABLE(0x7FFF, false, {
                // Not necessary to free, the key(s) is owned by the hashtable
                char *test_key_1_copy = (char*)xalloc_alloc(test_key_1_len + 1);
                strcpy(test_key_1_copy, test_key_1);

                hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));

                HASHTABLE_SET_KEY_DB_0_BY_INDEX(
                        chunk_index,
                        0,
                        test_key_1_hash,
                        test_key_1_copy,
                        test_key_1_len,
                        test_value_1);

                // Unset the flags and the data to simulate finding a value in the process of being set
                HASHTABLE_KEYS_VALUES(chunk_index, 0).flags =
                        HASHTABLE_KEY_VALUE_FLAG_DELETED;
                HASHTABLE_KEYS_VALUES(chunk_index, 0).data = 0;

                REQUIRE(!hashtable_mcmp_op_get(
                        hashtable,
                        0,
                        test_key_1,
                        test_key_1_len,
                        &value));
            })
        }

        SECTION("found - single bucket - get key after delete with hash still in hash_half (edge case because of parallelism)") {
            HASHTABLE(0x7FFF, false, {
                // Not necessary to free, the key(s) is owned by the hashtable
                char *test_key_1_copy = (char*)xalloc_alloc(test_key_1_len + 1);
                char *test_key_2_copy = (char*)xalloc_alloc(test_key_1_len + 1);
                strcpy(test_key_1_copy, test_key_1);
                strcpy(test_key_2_copy, test_key_1);

                hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));

                HASHTABLE_SET_KEY_DB_0_BY_INDEX(
                        chunk_index,
                        0,
                        test_key_1_hash,
                        test_key_1_copy,
                        test_key_1_len,
                        test_value_1);

                HASHTABLE_KEYS_VALUES(chunk_index, 0).flags =
                        HASHTABLE_KEY_VALUE_FLAG_DELETED;

                HASHTABLE_SET_KEY_DB_0_BY_INDEX(
                        chunk_index,
                        2,
                        test_key_1_hash,
                        test_key_2_copy,
                        test_key_1_len,
                        test_value_1 + 10);

                REQUIRE(hashtable_mcmp_op_get(
                        hashtable,
                        0,
                        test_key_1,
                        test_key_1_len,
                        &value));

                REQUIRE(value == test_value_1 + 10);
            })
        }
    }
}
