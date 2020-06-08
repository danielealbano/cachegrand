#include "catch.hpp"

#include <string.h>
#include <hashtable/hashtable.h>

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_support_index.h"
#include "hashtable/hashtable_op_set.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable_op_set.c", "[hashtable][hashtable_op][hashtable_op_set]") {
    SECTION("hashtable_op_set") {
        SECTION("set 1 bucket") {
            HASHTABLE(buckets_initial_count_5, false, {
                REQUIRE(hashtable_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_1));

                hashtable_bucket_index_t bucket_index = hashtable_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash);

                volatile hashtable_bucket_t* bucket = &hashtable->ht_current->buckets[bucket_index];

                // Check if the write lock has been released
                REQUIRE(bucket->write_lock == 0);

                // Check if the first slot of the chain ring contains the correct key/value
                REQUIRE(bucket->half_hashes[0] == test_key_1_hash_half);
                REQUIRE(bucket->keys_values[0].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)bucket->keys_values[0].inline_key.data,
                        test_key_1,
                        test_key_1_len) == 0);
                REQUIRE(bucket->keys_values[0].data == test_value_1);

                // Check if the subsequent element has been affected by the changes
                REQUIRE(bucket->half_hashes[1] == 0);
                REQUIRE(bucket->keys_values[1].flags == 0);
                REQUIRE(bucket->keys_values[1].inline_key.data[0] == 0);
            })
        }

        SECTION("set and update 1 slot") {
            HASHTABLE(buckets_initial_count_5, false, {
                REQUIRE(hashtable_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_1));

                REQUIRE(hashtable_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_1 + 1));

                hashtable_bucket_index_t bucket_index = hashtable_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash);

                volatile hashtable_bucket_t* bucket = &hashtable->ht_current->buckets[bucket_index];

                // Check if the first slot of the chain ring contains the correct key/value
                REQUIRE(bucket->half_hashes[0] == test_key_1_hash_half);
                REQUIRE(bucket->keys_values[0].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)bucket->keys_values[0].inline_key.data,
                        test_key_1,
                        test_key_1_len) == 0);
                REQUIRE(bucket->keys_values[0].data == test_value_1 + 1);

                // Check if the subsequent element has been affected by the changes
                REQUIRE(bucket->half_hashes[1] == 0);
                REQUIRE(bucket->keys_values[1].flags == 0);
                REQUIRE(bucket->keys_values[1].inline_key.data[0] == 0);
            })
        }

        SECTION("set 2 slots") {
            HASHTABLE(buckets_initial_count_5, false, {
                REQUIRE(hashtable_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_1));

                REQUIRE(hashtable_op_set(
                        hashtable,
                        test_key_2,
                        test_key_2_len,
                        test_value_2));

                hashtable_bucket_index_t bucket_index1 = hashtable_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash);

                hashtable_bucket_index_t bucket_index2 = hashtable_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_2_hash);

                volatile hashtable_bucket_t* bucket1 = &hashtable->ht_current->buckets[bucket_index1];
                volatile hashtable_bucket_t* bucket2 = &hashtable->ht_current->buckets[bucket_index2];

                // Check if the first slot of the chain ring contains the correct key/value
                REQUIRE(bucket1->half_hashes[0] == test_key_1_hash_half);
                REQUIRE(bucket1->keys_values[0].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)bucket1->keys_values[0].inline_key.data,
                        test_key_1,
                        test_key_1_len) == 0);
                REQUIRE(bucket1->keys_values[0].data == test_value_1);

                // Check if the first slot of the chain ring contains the correct key/value
                REQUIRE(bucket2->half_hashes[0] == test_key_2_hash_half);
                REQUIRE(bucket2->keys_values[0].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)bucket2->keys_values[0].inline_key.data,
                        test_key_2,
                        test_key_2_len) == 0);
                REQUIRE(bucket2->keys_values[0].data == test_value_2);
            })
        }

        SECTION("fill entire bucket - key with same prefix - key not inline") {
            HASHTABLE(buckets_initial_count_5, false, {
                for(uint32_t i = 0; i < HASHTABLE_BUCKET_SLOTS_COUNT; i++) {
                    REQUIRE(hashtable_op_set(
                            hashtable,
                            (char*)test_key_1_same_bucket[i].key,
                            test_key_1_same_bucket[i].key_len,
                            test_value_1 + i));
                }

                volatile hashtable_bucket_t* bucket = &hashtable->ht_current->buckets[test_index_1_buckets_count_42];

                for(uint32_t i = 0; i < HASHTABLE_BUCKET_SLOTS_COUNT; i++) {
                    REQUIRE(bucket->half_hashes[i] == test_key_1_same_bucket[i].key_hash_half);
                    REQUIRE(bucket->keys_values[i].flags == HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED);
                    REQUIRE(strncmp(
                            (char*)bucket->keys_values[i].prefix_key.data,
                            test_key_1_same_bucket[i].key,
                            HASHTABLE_KEY_PREFIX_SIZE) == 0);
                    REQUIRE(bucket->keys_values[i].data == test_value_1 + i);
                }
            })
        }

        SECTION("bucket overflow") {
            HASHTABLE(buckets_initial_count_5, false, {
                for(uint32_t i = 0; i < HASHTABLE_BUCKET_SLOTS_COUNT; i++) {
                    hashtable_op_set(
                            hashtable,
                            (char*)test_key_1_same_bucket[i].key,
                            test_key_1_same_bucket[i].key_len,
                            test_value_1 + i);
                }

                REQUIRE(!hashtable_op_set(
                        hashtable,
                        (char*)test_key_1_same_bucket[13].key,
                        test_key_1_same_bucket[13].key_len,
                        test_value_1 + 13));
            })
        }
    }
}
