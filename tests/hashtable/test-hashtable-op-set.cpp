#include <catch2/catch.hpp>
#include <numa.h>

#include <string.h>

#include "exttypes.h"
#include "spinlock.h"
#include "misc.h"
#include "log/log.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"
#include "data_structures/hashtable/mcmp/hashtable_support_index.h"
#include "data_structures/hashtable/mcmp/hashtable_op_set.h"

#include "../support.h"
#include "fixtures-hashtable.h"

TEST_CASE("hashtable/hashtable_mcmp_op_set.c", "[hashtable][hashtable_op][hashtable_mcmp_op_set]") {
    SECTION("hashtable_mcmp_op_set") {
        SECTION("set 1 bucket") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));
                hashtable_chunk_slot_index_t chunk_slot_index = 0;

                hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk =
                        &hashtable->ht_current->half_hashes_chunk[chunk_index];
                hashtable_key_value_volatile_t * key_value =
                        &hashtable->ht_current->keys_values[HASHTABLE_TO_BUCKET_INDEX(chunk_index, chunk_slot_index)];

                REQUIRE(hashtable_mcmp_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_1));

                // Check if the write lock has been released
                REQUIRE(!spinlock_is_locked(&half_hashes_chunk->write_lock));

                // Check if the first slot of the chain ring contains the correct key/value
                REQUIRE(half_hashes_chunk->metadata.changes_counter == 1);
                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].filled == true);
                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].distance == 0);
                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].quarter_hash == test_key_1_hash_quarter);
                REQUIRE(key_value->flags ==
                        (HASHTABLE_KEY_VALUE_FLAG_FILLED | HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)key_value->inline_key.data,
                        test_key_1,
                        test_key_1_len) == 0);
                REQUIRE(key_value->data == test_value_1);

                // Check if the subsequent element has been affected by the changes
                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index + 1].slot_id == 0);
            })
        }

        SECTION("set and update 1 slot") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));
                hashtable_chunk_slot_index_t chunk_slot_index = 0;

                hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk =
                        &hashtable->ht_current->half_hashes_chunk[chunk_index];
                hashtable_key_value_volatile_t * key_value =
                        &hashtable->ht_current->keys_values[HASHTABLE_TO_BUCKET_INDEX(chunk_index, chunk_slot_index)];

                REQUIRE(hashtable_mcmp_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_1));

                REQUIRE(hashtable_mcmp_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_1 + 1));
                // Check if the first slot of the chain ring contains the correct key/value
                REQUIRE(half_hashes_chunk->metadata.changes_counter == 2);
                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].filled == true);
                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].distance == 0);
                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].quarter_hash == test_key_1_hash_quarter);
                REQUIRE(key_value->flags ==
                        (HASHTABLE_KEY_VALUE_FLAG_FILLED | HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)key_value->inline_key.data,
                        test_key_1,
                        test_key_1_len) == 0);
                REQUIRE(key_value->data == test_value_1 + 1);

                // Check if the subsequent element has been affected by the changes
                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index + 1].slot_id == 0);
            })
        }

        SECTION("set 2 slots") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_index_t chunk_index1 = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));
                hashtable_chunk_slot_index_t chunk_slot_index1 = 0;

                hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk1 =
                        &hashtable->ht_current->half_hashes_chunk[chunk_index1];
                hashtable_key_value_volatile_t * key_value1 =
                        &hashtable->ht_current->keys_values[HASHTABLE_TO_BUCKET_INDEX(chunk_index1, chunk_slot_index1)];


                hashtable_chunk_index_t chunk_index2 = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_2_hash));
                hashtable_chunk_slot_index_t chunk_slot_index2 = 0;

                hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk2 =
                        &hashtable->ht_current->half_hashes_chunk[chunk_index2];
                hashtable_key_value_volatile_t * key_value2 =
                        &hashtable->ht_current->keys_values[HASHTABLE_TO_BUCKET_INDEX(chunk_index2, chunk_slot_index2)];

                REQUIRE(hashtable_mcmp_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_1));

                REQUIRE(hashtable_mcmp_op_set(
                        hashtable,
                        test_key_2,
                        test_key_2_len,
                        test_value_2));

                // Check the first set
                REQUIRE(half_hashes_chunk1->half_hashes[chunk_slot_index1].filled == true);
                REQUIRE(half_hashes_chunk1->half_hashes[chunk_slot_index1].distance == 0);
                REQUIRE(half_hashes_chunk1->half_hashes[chunk_slot_index1].quarter_hash == test_key_1_hash_quarter);
                REQUIRE(key_value1->flags ==
                        (HASHTABLE_KEY_VALUE_FLAG_FILLED | HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)key_value1->inline_key.data,
                        test_key_1,
                        test_key_1_len) == 0);
                REQUIRE(key_value1->data == test_value_1);

                // Check the second set
                REQUIRE(half_hashes_chunk2->half_hashes[chunk_slot_index2].filled == true);
                REQUIRE(half_hashes_chunk2->half_hashes[chunk_slot_index2].distance == 0);
                REQUIRE(half_hashes_chunk2->half_hashes[chunk_slot_index2].quarter_hash == test_key_2_hash_quarter);
                REQUIRE(key_value2->flags ==
                        (HASHTABLE_KEY_VALUE_FLAG_FILLED | HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)key_value2->inline_key.data,
                        test_key_2,
                        test_key_2_len) == 0);
                REQUIRE(key_value2->data == test_value_2);
            })
        }

        SECTION("fill entire half hashes chunk - key with same prefix - key not inline") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_slot_index_t slots_to_fill = HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;
                test_key_same_bucket_t *test_key_same_bucket = test_support_same_hash_mod_fixtures_generate(
                        hashtable->ht_current->buckets_count,
                        test_key_same_bucket_key_prefix_external,
                        slots_to_fill);

                for(hashtable_chunk_index_t i = 0; i < slots_to_fill; i++) {
                    REQUIRE(hashtable_mcmp_op_set(
                            hashtable,
                            (char *) test_key_same_bucket[i].key,
                            test_key_same_bucket[i].key_len,
                            test_value_1 + i));
                }

                hashtable_chunk_index_t chunk_index_base =
                        HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                                hashtable->ht_current->buckets_count,
                                test_key_same_bucket[0].key_hash));

                for(hashtable_chunk_index_t i = 0; i < slots_to_fill; i++) {
                    hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk =
                            &hashtable->ht_current->half_hashes_chunk[chunk_index_base];
                    hashtable_key_value_volatile_t * key_value =
                            &hashtable->ht_current->keys_values[HASHTABLE_TO_BUCKET_INDEX(chunk_index_base, i)];

                    REQUIRE(half_hashes_chunk->half_hashes[i].filled == true);
                    REQUIRE(half_hashes_chunk->half_hashes[i].distance == 0);
                    REQUIRE(half_hashes_chunk->half_hashes[i].quarter_hash == test_key_same_bucket[i].key_hash_quarter);
                    REQUIRE(key_value->flags == HASHTABLE_KEY_VALUE_FLAG_FILLED);

                    REQUIRE(strncmp(
                            (char*)key_value->external_key.data,
                            test_key_same_bucket[i].key,
                            key_value->external_key.size) == 0);

                    REQUIRE(key_value->data == test_value_1 + i);
                }

                test_support_same_hash_mod_fixtures_free(test_key_same_bucket);
            })
        }

        SECTION("overflow half hashes chunk - check hashes and key (key > INLINE, using prefix)") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_count_t chunks_to_overflow = 3;
                hashtable_chunk_slot_index_t slots_to_fill =
                        (HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT * chunks_to_overflow) + 3;
                test_key_same_bucket_t *test_key_same_bucket = test_support_same_hash_mod_fixtures_generate(
                        hashtable->ht_current->buckets_count,
                        test_key_same_bucket_key_prefix_external,
                        slots_to_fill);

                for(hashtable_chunk_slot_index_t i = 0; i < slots_to_fill; i++) {
                    REQUIRE(hashtable_mcmp_op_set(
                            hashtable,
                            (char *) test_key_same_bucket[i].key,
                            test_key_same_bucket[i].key_len,
                            test_value_1 + i));
                }

                hashtable_chunk_index_t chunk_index_base =
                        HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                                hashtable->ht_current->buckets_count,
                                test_key_same_bucket[0].key_hash));

                for(uint32_t i = 0; i < slots_to_fill; i++) {
                    hashtable_chunk_index_t chunk_index =
                            chunk_index_base + (int)(i / HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);
                    hashtable_chunk_slot_index_t chunk_slot_index =
                            i % HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;

                    hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk =
                            &hashtable->ht_current->half_hashes_chunk[chunk_index];

                    hashtable_key_value_volatile_t *key_value =
                            &hashtable->ht_current->keys_values[HASHTABLE_TO_BUCKET_INDEX(chunk_index, chunk_slot_index)];

                    REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].filled == true);
                    REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].distance == chunk_index - chunk_index_base);
                    REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].quarter_hash == test_key_same_bucket[i].key_hash_quarter);
                    REQUIRE(key_value->flags == HASHTABLE_KEY_VALUE_FLAG_FILLED);

                    REQUIRE(strncmp(
                            (char*)key_value->external_key.data,
                            test_key_same_bucket[i].key,
                            key_value->external_key.size) == 0);

                    REQUIRE(key_value->data == test_value_1 + i);
                }

                test_support_same_hash_mod_fixtures_free(test_key_same_bucket);
            })
        }

        SECTION("overflow half hashes chunk - check overflowed_chunks_counter") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_count_t chunks_to_overflow = 3;
                hashtable_chunk_slot_index_t slots_to_fill =
                        (HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT * chunks_to_overflow) + 3;
                test_key_same_bucket_t *test_key_same_bucket = test_support_same_hash_mod_fixtures_generate(
                        hashtable->ht_current->buckets_count,
                        test_key_same_bucket_key_prefix_external,
                        slots_to_fill);

                for(hashtable_chunk_slot_index_t i = 0; i < slots_to_fill; i++) {
                    REQUIRE(hashtable_mcmp_op_set(
                            hashtable,
                            (char *) test_key_same_bucket[i].key,
                            test_key_same_bucket[i].key_len,
                            test_value_1 + i));
                }

                hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_same_bucket[0].key_hash));

                hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk =
                        &hashtable->ht_current->half_hashes_chunk[chunk_index];
                REQUIRE(half_hashes_chunk->metadata.overflowed_chunks_counter == chunks_to_overflow);

                test_support_same_hash_mod_fixtures_free(test_key_same_bucket);
            })
        }

        SECTION("fill entire hashtable and fail") {
            HASHTABLE(0x7F, false, {
                uint32_t slots_to_fill = 448 + 1;
                test_key_same_bucket_t *test_key_same_bucket = test_support_same_hash_mod_fixtures_generate(
                        hashtable->ht_current->buckets_count,
                        test_key_same_bucket_key_prefix_external,
                        slots_to_fill);

                uint32_t i = 0;
                for(; i < slots_to_fill - 1; i++) {
                    REQUIRE(hashtable_mcmp_op_set(
                            hashtable,
                            (char *) test_key_same_bucket[i].key,
                            test_key_same_bucket[i].key_len,
                            test_value_1 + i));
                }

                REQUIRE(!hashtable_mcmp_op_set(
                        hashtable,
                        (char *) test_key_same_bucket[i].key,
                        test_key_same_bucket[i].key_len,
                        test_value_1 + i));

                test_support_same_hash_mod_fixtures_free(test_key_same_bucket);
            })
        }

        SECTION("set 1 bucket - numa aware") {
            if (numa_available() == 0 && numa_num_configured_nodes() >= 2) {
                HASHTABLE_NUMA_AWARE(0x7FFFF, false, numa_all_nodes_ptr, {
                    hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                            hashtable->ht_current->buckets_count,
                            test_key_1_hash));
                    hashtable_chunk_slot_index_t chunk_slot_index = 0;

                    hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk =
                            &hashtable->ht_current->half_hashes_chunk[chunk_index];
                    hashtable_key_value_volatile_t * key_value =
                            &hashtable->ht_current->keys_values[HASHTABLE_TO_BUCKET_INDEX(chunk_index, chunk_slot_index)];

                    REQUIRE(hashtable_mcmp_op_set(
                            hashtable,
                            test_key_1,
                            test_key_1_len,
                            test_value_1));

                    // Check if the write lock has been released
                    REQUIRE(!spinlock_is_locked(&half_hashes_chunk->write_lock));

                    // Check if the first slot of the chain ring contains the correct key/value
                    REQUIRE(half_hashes_chunk->metadata.changes_counter == 1);
                    REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].filled == true);
                    REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].distance == 0);
                    REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].quarter_hash == test_key_1_hash_quarter);
                    REQUIRE(key_value->flags ==
                            (HASHTABLE_KEY_VALUE_FLAG_FILLED | HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE));
                    REQUIRE(strncmp(
                            (char*)key_value->inline_key.data,
                            test_key_1,
                            test_key_1_len) == 0);
                    REQUIRE(key_value->data == test_value_1);

                    // Check if the subsequent element has been affected by the changes
                    REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index + 1].slot_id == 0);
                })
            } else {
                WARN("Can't test numa awareness, numa not available or only one numa node");
            }
        }

//        SECTION("parallel inserts - check storage") {
//            HASHTABLE(1000000, false, {
//                for(uint32_t i = 0; i < HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT; i++) {
//                    hashtable_mcmp_op_set(
//                            hashtable,
//                            (char*)test_key_1_same_bucket[i].key,
//                            test_key_1_same_bucket[i].key_len,
//                            test_value_1 + i);
//                }
//
//                REQUIRE(!hashtable_mcmp_op_set(
//                        hashtable,
//                        (char*)test_key_1_same_bucket[13].key,
//                        test_key_1_same_bucket[13].key_len,
//                        test_value_1 + 13));
//            })
//        }
    }
}
