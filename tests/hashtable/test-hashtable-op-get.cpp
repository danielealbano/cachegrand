#include <catch2/catch.hpp>

#include <string.h>

#include "exttypes.h"
#include "spinlock.h"
#include "xalloc.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_support_index.h"
#include "hashtable/hashtable_op_get.h"

#include "../support.h"
#include "fixtures-hashtable.h"

TEST_CASE("hashtable/hashtable_op_get.c", "[hashtable][hashtable_op][hashtable_op_get]") {
    SECTION("hashtable_op_get") {
        hashtable_value_data_t value = 0;

        SECTION("not found - hashtable empty") {
            HASHTABLE(0x7FFF, false, {
                REQUIRE(!hashtable_op_get(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        &value));
            })
        }

        SECTION("found - key inline") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));

                HASHTABLE_SET_KEY_INLINE_BY_INDEX(
                        chunk_index,
                        0,
                        test_key_1_hash,
                        test_key_1,
                        test_key_1_len,
                        test_value_1);

                REQUIRE(hashtable_op_get(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        &value));

                REQUIRE(value == test_value_1);
            })
        }

        SECTION("found - key external") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));

                HASHTABLE_SET_KEY_EXTERNAL_BY_INDEX(
                        chunk_index,
                        0,
                        test_key_1_hash,
                        test_key_1,
                        test_key_1_len,
                        test_value_1);

                REQUIRE(hashtable_op_get(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        &value));

                REQUIRE(value == test_value_1);
            })
        }

        SECTION("found - multiple chunks first slot") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_index_t chunk_index1 = HASHTABLE_TO_CHUNK_INDEX(hashtable_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));
                hashtable_chunk_index_t chunk_index2 = HASHTABLE_TO_CHUNK_INDEX(hashtable_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_2_hash));

                HASHTABLE_SET_KEY_INLINE_BY_INDEX(
                        chunk_index1,
                        0,
                        test_key_1_hash,
                        test_key_1,
                        test_key_1_len,
                        test_value_1);

                HASHTABLE_SET_KEY_INLINE_BY_INDEX(
                        chunk_index2,
                        0,
                        test_key_2_hash,
                        test_key_2,
                        test_key_2_len,
                        test_value_2);

                REQUIRE(hashtable_op_get(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        &value));

                REQUIRE(value == test_value_1);

                REQUIRE(hashtable_op_get(
                        hashtable,
                        test_key_2,
                        test_key_2_len,
                        &value));

                REQUIRE(value == test_value_2);
            })
        }

        SECTION("found - single chunk with first slot empty") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));

                HASHTABLE_SET_KEY_INLINE_BY_INDEX(
                        chunk_index,
                        1,
                        test_key_1_hash,
                        test_key_1,
                        test_key_1_len,
                        test_value_1);

                REQUIRE(hashtable_op_get(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        &value));

                REQUIRE(value == test_value_1);
            })
        }

        SECTION("found - single chunk multiple slots - key prefix/external") {
            HASHTABLE(0x7FFF, false, {
                test_key_same_bucket_t* test_key_same_bucket = test_support_same_hash_mod_fixtures_generate(
                        hashtable->ht_current->buckets_count,
                        test_key_same_bucket_key_prefix_external,
                        HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT);

                hashtable_bucket_index_t bucket_index_base =
                        test_key_same_bucket[0].key_hash % hashtable->ht_current->buckets_count;

                for(hashtable_chunk_slot_index_t i = 0; i < HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT; i++) {
                    HASHTABLE_SET_KEY_INLINE_BY_INDEX(
                            HASHTABLE_TO_CHUNK_INDEX(bucket_index_base),
                            i,
                            test_key_same_bucket[i].key_hash,
                            test_key_same_bucket[i].key,
                            test_key_same_bucket[i].key_len,
                            test_value_1 + i);
                }

                for(hashtable_chunk_slot_index_t i = 0; i < HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT; i++) {
                    REQUIRE(hashtable_op_get(
                            hashtable,
                            (char*)test_key_same_bucket[i].key,
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
                        (HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT * chunks_to_set) + 3;
                test_key_same_bucket_t* test_key_same_bucket = test_support_same_hash_mod_fixtures_generate(
                        hashtable->ht_current->buckets_count,
                        test_key_same_bucket_key_prefix_external,
                        slots_to_fill);

                hashtable_bucket_index_t bucket_index_base =
                        test_key_same_bucket[0].key_hash % hashtable->ht_current->buckets_count;
                hashtable_chunk_index_t chunk_index_base = HASHTABLE_TO_CHUNK_INDEX(bucket_index_base);

                hashtable->ht_current->half_hashes_chunk[chunk_index_base].metadata.overflowed_chunks_counter =
                        ceil((double)slots_to_fill / HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT);


                for(hashtable_chunk_slot_index_t i = 0; i < slots_to_fill; i++) {
                    hashtable_chunk_index_t chunk_index =
                            chunk_index_base + (int)(i / HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT);
                    hashtable_chunk_slot_index_t chunk_slot_index =
                            i % HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT;

                    HASHTABLE_SET_KEY_INLINE_BY_INDEX(
                            chunk_index,
                            chunk_slot_index,
                            test_key_same_bucket[i].key_hash,
                            test_key_same_bucket[i].key,
                            test_key_same_bucket[i].key_len,
                            test_value_1 + i);
                    HASHTABLE_HALF_HASHES_CHUNK(chunk_index).half_hashes[chunk_slot_index].distance =
                            chunk_index - chunk_index_base;
                }

                for(hashtable_chunk_slot_index_t i = 0; i < slots_to_fill; i++) {
                    hashtable_chunk_index_t chunk_index =
                            chunk_index_base + (int)(i / HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT);
                    hashtable_chunk_slot_index_t chunk_slot_index =
                            i % HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT;

                    REQUIRE(hashtable_op_get(
                            hashtable,
                            (char*)test_key_same_bucket[i].key,
                            test_key_same_bucket[i].key_len,
                            &value));
                    REQUIRE(value == test_value_1 + i);
                }

                test_support_same_hash_mod_fixtures_free(test_key_same_bucket);
            })
        }

        SECTION("not found - deleted flag") {
            HASHTABLE(0x7FFF, false, {
                HASHTABLE_SET_KEY_INLINE_BY_INDEX(
                        HASHTABLE_TO_CHUNK_INDEX(test_key_1_hash & (0x8000 - 1)),
                        0,
                        test_key_1_hash,
                        test_key_1,
                        test_key_1_len,
                        test_value_1);

                HASHTABLE_KEYS_VALUES(HASHTABLE_TO_CHUNK_INDEX(test_key_1_hash & (0x8000 - 1)), 0).flags =
                        HASHTABLE_KEY_VALUE_FLAG_DELETED;

                REQUIRE(!hashtable_op_get(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        &value));
            })
        }

        SECTION("not found - hash set but key_value not (edge case because of parallelism)") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));

                HASHTABLE_SET_KEY_INLINE_BY_INDEX(
                        chunk_index,
                        0,
                        test_key_1_hash,
                        test_key_1,
                        test_key_1_len,
                        test_value_1);

                // Unset the flags and the data to simulate finding a value in the process of being set
                HASHTABLE_KEYS_VALUES(chunk_index, 0).flags =
                        HASHTABLE_KEY_VALUE_FLAG_DELETED;
                HASHTABLE_KEYS_VALUES(chunk_index, 0).data = 0;

                REQUIRE(!hashtable_op_get(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        &value));
            })
        }

        SECTION("found - single bucket - get key after delete with hash still in hash_half (edge case because of parallelism)") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));

                HASHTABLE_SET_KEY_INLINE_BY_INDEX(
                        chunk_index,
                        0,
                        test_key_1_hash,
                        test_key_1,
                        test_key_1_len,
                        test_value_1);

                HASHTABLE_KEYS_VALUES(chunk_index, 0).flags =
                        HASHTABLE_KEY_VALUE_FLAG_DELETED;

                HASHTABLE_SET_KEY_INLINE_BY_INDEX(
                        chunk_index,
                        2,
                        test_key_1_hash,
                        test_key_1,
                        test_key_1_len,
                        test_value_1 + 10);

                REQUIRE(hashtable_op_get(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        &value));

                REQUIRE(value == test_value_1 + 10);
            })
        }
    }
}
