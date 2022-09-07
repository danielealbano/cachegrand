/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>
#include <numa.h>

#include <string.h>

#include "exttypes.h"
#include "spinlock.h"
#include "xalloc.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "random.h"
#include "fiber.h"
#include "fiber_scheduler.h"
#include "clock.h"
#include "config.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"
#include "data_structures/hashtable/mcmp/hashtable_support_index.h"
#include "data_structures/hashtable/mcmp/hashtable_op_set.h"
#include "data_structures/hashtable/mcmp/hashtable_op_delete.h"

#include "../../../support.h"
#include "fixtures-hashtable-mpmc.h"

TEST_CASE("hashtable/hashtable_mcmp_op_delete.c", "[hashtable][hashtable_op][hashtable_mcmp_op_delete]") {
    worker_context_t worker_context = { 0 };
    worker_context.worker_index = UINT16_MAX;
    worker_context_set(&worker_context);
    transaction_set_worker_index(worker_context.worker_index);

    SECTION("hashtable_mcmp_op_delete") {
        SECTION("delete non-existing") {
            HASHTABLE(0x7FFF, false, {
                REQUIRE(!hashtable_mcmp_op_delete(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        NULL));
            })
        }

        SECTION("set and delete 1 bucket") {
            HASHTABLE(0x7FFF, false, {
                uintptr_t prev_value = 0;
                hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));
                hashtable_chunk_slot_index_t chunk_slot_index = 0;

                hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk =
                        &hashtable->ht_current->half_hashes_chunk[chunk_index];
                hashtable_key_value_volatile_t *key_value =
                        &hashtable->ht_current->keys_values[HASHTABLE_TO_BUCKET_INDEX(chunk_index, chunk_slot_index)];

                char *test_key_1_alloc = (char*)xalloc_alloc(test_key_1_len + 1);
                strncpy(test_key_1_alloc, test_key_1, test_key_1_len + 1);

                REQUIRE(hashtable_mcmp_op_set(
                        hashtable,
                        test_key_1_alloc,
                        test_key_1_len,
                        test_value_1,
                        NULL));

                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].quarter_hash == test_key_1_hash_quarter);
                REQUIRE(key_value->flags != HASHTABLE_KEY_VALUE_FLAG_DELETED);

                REQUIRE(hashtable_mcmp_op_delete(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        NULL));

                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].slot_id == 0);
                REQUIRE(key_value->flags == HASHTABLE_KEY_VALUE_FLAG_DELETED);
            })
        }

        SECTION("set and delete 1 bucket - check previous value") {
            HASHTABLE(0x7FFF, false, {
                uintptr_t prev_value;
                hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));
                hashtable_chunk_slot_index_t chunk_slot_index = 0;

                hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk =
                        &hashtable->ht_current->half_hashes_chunk[chunk_index];
                hashtable_key_value_volatile_t *key_value =
                        &hashtable->ht_current->keys_values[HASHTABLE_TO_BUCKET_INDEX(chunk_index, chunk_slot_index)];

                char *test_key_1_alloc = (char*)xalloc_alloc(test_key_1_len + 1);
                strncpy(test_key_1_alloc, test_key_1, test_key_1_len + 1);

                REQUIRE(hashtable_mcmp_op_set(
                        hashtable,
                        test_key_1_alloc,
                        test_key_1_len,
                        test_value_1,
                        NULL));

                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].quarter_hash == test_key_1_hash_quarter);
                REQUIRE(key_value->flags != HASHTABLE_KEY_VALUE_FLAG_DELETED);

                REQUIRE(hashtable_mcmp_op_delete(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        &prev_value));

                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].slot_id == 0);
                REQUIRE(key_value->flags == HASHTABLE_KEY_VALUE_FLAG_DELETED);
                REQUIRE(prev_value == test_value_1);
            })
        }

        SECTION("set and delete 1 bucket - twice to reuse") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));
                hashtable_chunk_slot_index_t chunk_slot_index = 0;

                hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk =
                        &hashtable->ht_current->half_hashes_chunk[chunk_index];
                hashtable_key_value_volatile_t *key_value =
                        &hashtable->ht_current->keys_values[HASHTABLE_TO_BUCKET_INDEX(chunk_index, chunk_slot_index)];

                char *test_key_1_alloc = (char*)xalloc_alloc(test_key_1_len + 1);
                strncpy(test_key_1_alloc, test_key_1, test_key_1_len + 1);

                REQUIRE(hashtable_mcmp_op_set(
                        hashtable,
                        test_key_1_alloc,
                        test_key_1_len,
                        test_value_1,
                        NULL));

                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].quarter_hash == test_key_1_hash_quarter);
                REQUIRE(key_value->flags != HASHTABLE_KEY_VALUE_FLAG_DELETED);

                REQUIRE(hashtable_mcmp_op_delete(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        NULL));

                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].slot_id == 0);
                REQUIRE(key_value->flags == HASHTABLE_KEY_VALUE_FLAG_DELETED);

                test_key_1_alloc = (char*)xalloc_alloc(test_key_1_len + 1);
                strncpy(test_key_1_alloc, test_key_1, test_key_1_len + 1);

                REQUIRE(hashtable_mcmp_op_set(
                        hashtable,
                        test_key_1_alloc,
                        test_key_1_len,
                        test_value_1,
                        NULL));

                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].filled == true);
                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].distance == 0);
                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].quarter_hash == test_key_1_hash_quarter);
                REQUIRE(key_value->flags != HASHTABLE_KEY_VALUE_FLAG_DELETED);

                REQUIRE(hashtable_mcmp_op_delete(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        NULL));

                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].slot_id == 0);
                REQUIRE(key_value->flags == HASHTABLE_KEY_VALUE_FLAG_DELETED);
            })
        }

        SECTION("set N buckets delete random") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_slot_index_t slots_to_fill = 8;
                test_key_same_bucket_t* test_key_same_bucket = test_support_same_hash_mod_fixtures_generate(
                        hashtable->ht_current->buckets_count,
                        test_key_same_bucket_key_prefix_external,
                        slots_to_fill);

                for(hashtable_chunk_index_t i = 0; i < slots_to_fill; i++) {
                    char *test_key_same_bucket_alloc = (char*)xalloc_alloc(test_key_same_bucket[i].key_len + 1);
                    strncpy(test_key_same_bucket_alloc, test_key_same_bucket[i].key, test_key_same_bucket[i].key_len + 1);

                    REQUIRE(hashtable_mcmp_op_set(
                            hashtable,
                            test_key_same_bucket_alloc,
                            test_key_same_bucket[i].key_len,
                            test_value_1 + i,
                            NULL));
                }

                hashtable_chunk_slot_index_t random_slot_index = random_generate() % slots_to_fill;

                REQUIRE(hashtable_mcmp_op_delete(
                        hashtable,
                        test_key_same_bucket[random_slot_index].key,
                        test_key_same_bucket[random_slot_index].key_len,
                        NULL));

                hashtable_chunk_index_t chunk_index_base =
                        HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                                hashtable->ht_current->buckets_count,
                                test_key_same_bucket[0].key_hash));
                hashtable_half_hashes_chunk_volatile_t* half_hashes_chunk =
                        &hashtable->ht_current->half_hashes_chunk[chunk_index_base];
                hashtable_key_value_volatile_t * key_value =
                        &hashtable->ht_current->keys_values[HASHTABLE_TO_BUCKET_INDEX(chunk_index_base, random_slot_index)];

                REQUIRE(half_hashes_chunk->half_hashes[random_slot_index].slot_id == 0);
                REQUIRE(key_value->flags == HASHTABLE_KEY_VALUE_FLAG_DELETED);
                REQUIRE(key_value->data == test_value_1 + random_slot_index);

                // The delete operation in the hash table already frees all the keys so it's enough to free test_key_same_bucket
                free(test_key_same_bucket);
            })
        }

        SECTION("set N buckets delete random and re-insert") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_slot_index_t slots_to_fill = 8;
                test_key_same_bucket_t* test_key_same_bucket = test_support_same_hash_mod_fixtures_generate(
                        hashtable->ht_current->buckets_count,
                        test_key_same_bucket_key_prefix_external,
                        slots_to_fill);

                for(hashtable_chunk_index_t i = 0; i < slots_to_fill - 1; i++) {
                    char *test_key_same_bucket_alloc = (char*)xalloc_alloc(test_key_same_bucket[i].key_len + 1);
                    strncpy(test_key_same_bucket_alloc, test_key_same_bucket[i].key, test_key_same_bucket[i].key_len + 1);

                    REQUIRE(hashtable_mcmp_op_set(
                            hashtable,
                            test_key_same_bucket_alloc,
                            test_key_same_bucket[i].key_len,
                            test_value_1 + i,
                            NULL));
                }

                hashtable_chunk_slot_index_t random_slot_index = random_generate() % (slots_to_fill - 1);

                REQUIRE(hashtable_mcmp_op_delete(
                        hashtable,
                        test_key_same_bucket[random_slot_index].key,
                        test_key_same_bucket[random_slot_index].key_len,
                        NULL));

                hashtable_chunk_index_t chunk_index_base =
                        HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                                hashtable->ht_current->buckets_count,
                                test_key_same_bucket[0].key_hash));
                hashtable_half_hashes_chunk_volatile_t* half_hashes_chunk =
                        &hashtable->ht_current->half_hashes_chunk[chunk_index_base];
                hashtable_key_value_volatile_t * key_value =
                        &hashtable->ht_current->keys_values[HASHTABLE_TO_BUCKET_INDEX(chunk_index_base, random_slot_index)];

                REQUIRE(half_hashes_chunk->half_hashes[random_slot_index].slot_id == 0);
                REQUIRE(key_value->flags == HASHTABLE_KEY_VALUE_FLAG_DELETED);
                REQUIRE(key_value->data == test_value_1 + random_slot_index);

                char *test_key_same_bucket_alloc = (char*)xalloc_alloc(test_key_same_bucket[slots_to_fill - 1].key_len + 1);
                strncpy(test_key_same_bucket_alloc, test_key_same_bucket[slots_to_fill - 1].key, test_key_same_bucket[slots_to_fill - 1].key_len + 1);

                REQUIRE(hashtable_mcmp_op_set(
                        hashtable,
                        test_key_same_bucket_alloc,
                        test_key_same_bucket[slots_to_fill - 1].key_len,
                        test_value_1 + slots_to_fill - 1,
                        NULL));

                REQUIRE(half_hashes_chunk->half_hashes[random_slot_index].filled == true);
                REQUIRE(half_hashes_chunk->half_hashes[random_slot_index].distance == 0);
                REQUIRE(half_hashes_chunk->half_hashes[random_slot_index].quarter_hash ==
                        test_key_same_bucket[slots_to_fill - 1].key_hash_quarter);
                REQUIRE(key_value->flags == HASHTABLE_KEY_VALUE_FLAG_FILLED);
                REQUIRE(key_value->data == test_value_1 + slots_to_fill - 1);

                // The delete operation in the hash table already frees all the keys so it's enough to free test_key_same_bucket
                free(test_key_same_bucket);
            })
        }
    }
}
