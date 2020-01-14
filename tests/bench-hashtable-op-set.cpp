#include "catch.hpp"

#include <string.h>
#include <hashtable/hashtable.h>

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_support_index.h"
#include "hashtable/hashtable_op_set.h"

#include "fixtures-hashtable.h"

TEST_CASE("bench - hashtable_op_set.c", "[bench][hashtable][hashtable_op][hashtable_op_set]") {
    SECTION("hashtable_op_set") {
        SECTION("set 1 bucket") {
            HASHTABLE_INIT(buckets_initial_count_5, false, {
                hashtable_bucket_index_t index_neighborhood_begin, index_neighborhood_end;

                hashtable_support_index_calculate_neighborhood_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash,
                        &index_neighborhood_begin,
                        &index_neighborhood_end);

                REQUIRE(hashtable_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_1));

                REQUIRE(hashtable->ht_current->hashes[index_neighborhood_begin] == test_key_1_hash);
                REQUIRE(hashtable->ht_current->keys_values[index_neighborhood_begin].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)hashtable->ht_current->keys_values[index_neighborhood_begin].inline_key.data,
                        test_key_1,
                        test_key_1_len) == 0);
                REQUIRE(hashtable->ht_current->keys_values[index_neighborhood_begin].data == test_value_1);
            })
        }

        SECTION("set and update 1 bucket") {
            HASHTABLE_INIT(buckets_initial_count_5, false, {
                hashtable_bucket_index_t index_neighborhood_begin, index_neighborhood_end;

                hashtable_support_index_calculate_neighborhood_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash,
                        &index_neighborhood_begin,
                        &index_neighborhood_end);

                REQUIRE(hashtable_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_1));

                REQUIRE(hashtable_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_2));

                REQUIRE(hashtable->ht_current->hashes[index_neighborhood_begin] == test_key_1_hash);
                REQUIRE(hashtable->ht_current->keys_values[index_neighborhood_begin].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)hashtable->ht_current->keys_values[index_neighborhood_begin].inline_key.data,
                        test_key_1,
                        test_key_1_len) == 0);
                REQUIRE(hashtable->ht_current->keys_values[index_neighborhood_begin].data == test_value_2);
            })
        }

        SECTION("set 2 buckets") {
            HASHTABLE_INIT(buckets_initial_count_5, false, {
                hashtable_bucket_index_t index_1_neighborhood_begin, index_1_neighborhood_end;
                hashtable_bucket_index_t index_2_neighborhood_begin, index_2_neighborhood_end;

                hashtable_support_index_calculate_neighborhood_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash,
                        &index_1_neighborhood_begin,
                        &index_1_neighborhood_end);

                hashtable_support_index_calculate_neighborhood_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_2_hash,
                        &index_2_neighborhood_begin,
                        &index_2_neighborhood_end);

                REQUIRE(index_1_neighborhood_begin != index_2_neighborhood_begin);

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

                REQUIRE(hashtable->ht_current->hashes[index_1_neighborhood_begin] == test_key_1_hash);
                REQUIRE(hashtable->ht_current->keys_values[index_1_neighborhood_begin].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)hashtable->ht_current->keys_values[index_1_neighborhood_begin].inline_key.data,
                        test_key_1,
                        test_key_1_len) == 0);
                REQUIRE(hashtable->ht_current->keys_values[index_1_neighborhood_begin].data == test_value_1);

                REQUIRE(hashtable->ht_current->hashes[index_2_neighborhood_begin] == test_key_2_hash);
                REQUIRE(hashtable->ht_current->keys_values[index_2_neighborhood_begin].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)hashtable->ht_current->keys_values[index_2_neighborhood_begin].inline_key.data,
                        test_key_2,
                        test_key_2_len) == 0);
                REQUIRE(hashtable->ht_current->keys_values[index_2_neighborhood_begin].data == test_value_2);
            })
        }

        SECTION("set delete set 1 bucket") {
            HASHTABLE_INIT(buckets_initial_count_5, false, {
                hashtable_bucket_index_t index_1_neighborhood_begin, index_1_neighborhood_end;
                hashtable_bucket_index_t index_2_neighborhood_begin, index_2_neighborhood_end;

                hashtable_support_index_calculate_neighborhood_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash,
                        &index_1_neighborhood_begin,
                        &index_1_neighborhood_end);

                hashtable_support_index_calculate_neighborhood_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_2_hash,
                        &index_2_neighborhood_begin,
                        &index_2_neighborhood_end);

                REQUIRE(index_1_neighborhood_begin != index_2_neighborhood_begin);

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

                REQUIRE(hashtable->ht_current->hashes[index_1_neighborhood_begin] == test_key_1_hash);
                REQUIRE(hashtable->ht_current->keys_values[index_1_neighborhood_begin].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)hashtable->ht_current->keys_values[index_1_neighborhood_begin].inline_key.data,
                        test_key_1,
                        test_key_1_len) == 0);
                REQUIRE(hashtable->ht_current->keys_values[index_1_neighborhood_begin].data == test_value_1);

                REQUIRE(hashtable->ht_current->hashes[index_2_neighborhood_begin] == test_key_2_hash);
                REQUIRE(hashtable->ht_current->keys_values[index_2_neighborhood_begin].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)hashtable->ht_current->keys_values[index_2_neighborhood_begin].inline_key.data,
                        test_key_2,
                        test_key_2_len) == 0);
                REQUIRE(hashtable->ht_current->keys_values[index_2_neighborhood_begin].data == test_value_2);
            })
        }

    }
}
