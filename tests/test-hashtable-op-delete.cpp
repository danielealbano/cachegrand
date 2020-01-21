#include "catch.hpp"

#include <string.h>
#include <hashtable/hashtable.h>

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_support_index.h"
#include "hashtable/hashtable_op_set.h"
#include "hashtable/hashtable_op_delete.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable_op_delete.c", "[hashtable][hashtable_op][hashtable_op_delete]") {
    SECTION("hashtable_op_delete") {
        SECTION("set and delete 1 bucket") {
            HASHTABLE(buckets_initial_count_5, false, {
                hashtable_bucket_index_t index_neighborhood_begin, index_neighborhood_end;

                hashtable_support_index_calculate_neighborhood_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash,
                        hashtable->ht_current->cachelines_to_probe,
                        &index_neighborhood_begin,
                        &index_neighborhood_end);

                REQUIRE(hashtable_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_1));

                REQUIRE(hashtable->ht_current->hashes[index_neighborhood_begin] == test_key_1_hash);

                REQUIRE(hashtable_op_delete(
                        hashtable,
                        test_key_1,
                        test_key_1_len));

                REQUIRE(hashtable->ht_current->hashes[index_neighborhood_begin] == 0);
                REQUIRE(hashtable->ht_current->keys_values[index_neighborhood_begin].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED));
            })
        }

        SECTION("set and delete 1 bucket - twice to reuse") {
            HASHTABLE(buckets_initial_count_5, false, {
                hashtable_bucket_index_t index_neighborhood_begin, index_neighborhood_end;

                hashtable_support_index_calculate_neighborhood_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash,
                        hashtable->ht_current->cachelines_to_probe,
                        &index_neighborhood_begin,
                        &index_neighborhood_end);

                REQUIRE(hashtable_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_1));

                REQUIRE(hashtable->ht_current->hashes[index_neighborhood_begin] == test_key_1_hash);

                REQUIRE(hashtable_op_delete(
                        hashtable,
                        test_key_1,
                        test_key_1_len));

                REQUIRE(hashtable->ht_current->hashes[index_neighborhood_begin] == 0);
                REQUIRE(hashtable->ht_current->keys_values[index_neighborhood_begin].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED));

                REQUIRE(hashtable_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_1));

                REQUIRE(hashtable->ht_current->hashes[index_neighborhood_begin] == test_key_1_hash);

                REQUIRE(hashtable_op_delete(
                        hashtable,
                        test_key_1,
                        test_key_1_len));

                REQUIRE(hashtable->ht_current->hashes[index_neighborhood_begin] == 0);
                REQUIRE(hashtable->ht_current->keys_values[index_neighborhood_begin].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED));
            })
        }

        SECTION("set 2 buckets delete second") {
            HASHTABLE(buckets_initial_count_5, false, {
                hashtable_bucket_index_t index_1_neighborhood_begin, index_1_neighborhood_end;
                hashtable_bucket_index_t index_2_neighborhood_begin, index_2_neighborhood_end;

                hashtable_support_index_calculate_neighborhood_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash,
                        hashtable->ht_current->cachelines_to_probe,
                        &index_1_neighborhood_begin,
                        &index_1_neighborhood_end);

                hashtable_support_index_calculate_neighborhood_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_2_hash,
                        hashtable->ht_current->cachelines_to_probe,
                        &index_2_neighborhood_begin,
                        &index_2_neighborhood_end);

                if (index_1_neighborhood_begin == index_2_neighborhood_begin) {
                    index_2_neighborhood_begin++;
                }

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
                REQUIRE(hashtable->ht_current->hashes[index_2_neighborhood_begin] == test_key_2_hash);

                REQUIRE(hashtable_op_delete(
                        hashtable,
                        test_key_2,
                        test_key_2_len));

                REQUIRE(hashtable->ht_current->hashes[index_2_neighborhood_begin] == 0);
                REQUIRE(hashtable->ht_current->keys_values[index_2_neighborhood_begin].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED));

            })
        }
    }
}
