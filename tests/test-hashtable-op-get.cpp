#include "catch.hpp"

#include <string.h>

#include "xalloc.h"
#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_support_index.h"
#include "hashtable/hashtable_op_get.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable_op_get.c", "[hashtable][hashtable_op_get]") {
    SECTION("hashtable_op_get") {
        hashtable_value_data_t value = 0;

        SECTION("not found - hashtable empty") {
            HASHTABLE(buckets_initial_count_5, false, {
                REQUIRE(!hashtable_op_get(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        &value));
            })
        }

        SECTION("found - test key inline") {
            HASHTABLE(buckets_initial_count_5, false, {
                HASHTABLE_BUCKET_NEW_KEY_INLINE(
                        test_index_1_buckets_count_42,
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

        SECTION("found - test key external") {
            HASHTABLE(buckets_initial_count_5, false, {
                HASHTABLE_BUCKET_NEW_KEY_EXTERNAL(
                        test_index_1_buckets_count_42,
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

        SECTION("found - test multiple buckets") {
            HASHTABLE(buckets_initial_count_5, false, {
                HASHTABLE_BUCKET_KEYS_VALUES_NEW(test_index_1_buckets_count_42);
                HASHTABLE_BUCKET_SET_INDEX_KEY_INLINE(
                        test_index_1_buckets_count_42,
                        0,
                        test_key_1_hash,
                        test_key_1,
                        test_key_1_len,
                        test_value_1);

                HASHTABLE_BUCKET_KEYS_VALUES_NEW(test_index_2_buckets_count_42);
                HASHTABLE_BUCKET_SET_INDEX_KEY_INLINE(
                        test_index_2_buckets_count_42,
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

        SECTION("found - test single bucket with first slot empty") {
            HASHTABLE(buckets_initial_count_5, false, {
                HASHTABLE_BUCKET_KEYS_VALUES_NEW(test_index_1_buckets_count_42);
                HASHTABLE_BUCKET_SET_INDEX_KEY_INLINE(
                        test_index_1_buckets_count_42,
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

        SECTION("found - test single bucket multiple slots with key inline") {
            HASHTABLE(buckets_initial_count_5, false, {
                HASHTABLE_BUCKET_KEYS_VALUES_NEW(test_index_1_buckets_count_42);

                for(hashtable_bucket_slot_index_t i = 0; i < HASHTABLE_BUCKET_SLOTS_COUNT; i++) {
                    HASHTABLE_BUCKET_SET_INDEX_KEY_INLINE(
                            test_index_1_buckets_count_42,
                            i,
                            test_key_1_same_bucket[i].key_hash,
                            test_key_1_same_bucket[i].key,
                            test_key_1_same_bucket[i].key_len,
                            test_value_1 + i);
                }


                for(hashtable_bucket_slot_index_t i = 0; i < HASHTABLE_BUCKET_SLOTS_COUNT; i++) {
                    REQUIRE(hashtable_op_get(
                            hashtable,
                            (char*)test_key_1_same_bucket[i].key,
                            test_key_1_same_bucket[i].key_len,
                            &value));
                    REQUIRE(value == test_value_1 + i);
                }
            })
        }

        SECTION("not found - test deleted flag") {
            HASHTABLE(buckets_initial_count_5, false, {
                HASHTABLE_BUCKET_KEYS_VALUES_NEW(test_index_1_buckets_count_42);

                HASHTABLE_BUCKET_SET_INDEX_KEY_INLINE(
                        test_index_1_buckets_count_42,
                        0,
                        test_key_1_hash,
                        test_key_1,
                        test_key_1_len,
                        test_value_1);

                HASHTABLE_BUCKET_KEY_VALUE_SET_FLAG(
                        HASHTABLE_BUCKET(test_index_1_buckets_count_42).keys_values[0].flags,
                        HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED);

                REQUIRE(!hashtable_op_get(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        &value));
            })
        }

        SECTION("not found - test hash set but key_value not (edge case because of parallelism)") {
            HASHTABLE(buckets_initial_count_5, false, {
                HASHTABLE_BUCKET_KEYS_VALUES_NEW(test_index_1_buckets_count_42);

                HASHTABLE_BUCKET_SET_INDEX_KEY_INLINE(
                        test_index_1_buckets_count_42,
                        0,
                        test_key_1_hash,
                        test_key_1,
                        test_key_1_len,
                        test_value_1);

                // Unset the flags and the data to simulate finding a value in the process of being set
                HASHTABLE_BUCKET(test_index_1_buckets_count_42).keys_values[0].flags = 0;
                HASHTABLE_BUCKET(test_index_1_buckets_count_42).keys_values[0].data = 0;

                REQUIRE(!hashtable_op_get(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        &value));
            })
        }

        SECTION("found - single bucket - get key after delete with hash still in hash_half (edge case because of parallelism)") {
            HASHTABLE(buckets_initial_count_5, false, {
                HASHTABLE_BUCKET_KEYS_VALUES_NEW(test_index_1_buckets_count_42);

                HASHTABLE_BUCKET_SET_INDEX_KEY_INLINE(
                        test_index_1_buckets_count_42,
                        0,
                        test_key_1_hash,
                        test_key_1,
                        test_key_1_len,
                        test_value_1);

                HASHTABLE_BUCKET_KEY_VALUE_SET_FLAG(
                        HASHTABLE_BUCKET(test_index_1_buckets_count_42).keys_values[0].flags,
                        HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED);

                HASHTABLE_BUCKET_SET_INDEX_KEY_INLINE(
                        test_index_1_buckets_count_42,
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
