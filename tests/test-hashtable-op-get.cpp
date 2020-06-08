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

        SECTION("found - test multiple buckets single chain with single key inline") {
            HASHTABLE(buckets_initial_count_5, false, {
                hashtable_bucket_chain_ring_t* chain_ring;

                chain_ring = HASHTABLE_BUCKET_CHAIN_RING_NEW();
                HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_INLINE(
                        chain_ring,
                        1,
                        test_key_1_hash,
                        test_key_1,
                        test_key_1_len,
                        test_value_1);
                hashtable->ht_current->buckets[test_index_1_buckets_count_42].chain_first_ring = chain_ring;

                chain_ring = HASHTABLE_BUCKET_CHAIN_RING_NEW();
                HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_INLINE(
                        chain_ring,
                        2,
                        test_key_2_hash,
                        test_key_2,
                        test_key_2_len,
                        test_value_2);
                hashtable->ht_current->buckets[test_index_2_buckets_count_42].chain_first_ring = chain_ring;

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

        SECTION("found - test single bucket single chain multiple with key inline") {
            HASHTABLE(buckets_initial_count_5, false, {
                hashtable_bucket_chain_ring_t* chain_ring = HASHTABLE_BUCKET_CHAIN_RING_NEW();
                hashtable->ht_current->buckets[test_index_1_buckets_count_42].chain_first_ring = chain_ring;

                for(uint32_t i = 0; i < HASHTABLE_BUCKET_CHAIN_RING_SLOTS_COUNT; i++) {
                    HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_INLINE(
                            chain_ring,
                            i,
                            test_key_1_same_bucket[i].key_hash,
                            test_key_1_same_bucket[i].key,
                            test_key_1_same_bucket[i].key_len,
                            test_value_1 + i);
                }


                for(uint32_t i = 0; i < HASHTABLE_BUCKET_CHAIN_RING_SLOTS_COUNT; i++) {
                    REQUIRE(hashtable_op_get(
                            hashtable,
                            (char*)test_key_1_same_bucket[i].key,
                            test_key_1_same_bucket[i].key_len,
                            &value));
                    REQUIRE(value == test_value_1 + i);
                }
            })
        }

        SECTION("found - test single bucket multiple chain multiple with key inline") {
            HASHTABLE(buckets_initial_count_5, false, {
                hashtable_bucket_chain_ring_t* chain_ring1 = HASHTABLE_BUCKET_CHAIN_RING_NEW();
                hashtable_bucket_chain_ring_t* chain_ring2 = HASHTABLE_BUCKET_CHAIN_RING_NEW();
                hashtable_bucket_chain_ring_t* chain_ring3 = HASHTABLE_BUCKET_CHAIN_RING_NEW();

                chain_ring1->next_ring = chain_ring2;
                chain_ring2->next_ring = chain_ring3;
                hashtable->ht_current->buckets[test_index_1_buckets_count_42].chain_first_ring = chain_ring1;

                HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_INLINE(
                        chain_ring1,
                        1,
                        test_key_1_same_bucket[1].key_hash,
                        test_key_1_same_bucket[1].key,
                        test_key_1_same_bucket[1].key_len,
                        test_value_1 + 1);
                HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_INLINE(
                        chain_ring1,
                        2,
                        test_key_1_same_bucket[2].key_hash,
                        test_key_1_same_bucket[2].key,
                        test_key_1_same_bucket[2].key_len,
                        test_value_1 + 2);
                HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_INLINE(
                        chain_ring2,
                        3,
                        test_key_1_same_bucket[3].key_hash,
                        test_key_1_same_bucket[3].key,
                        test_key_1_same_bucket[3].key_len,
                        test_value_1 + 3);
                HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_INLINE(
                        chain_ring3,
                        4,
                        test_key_1_same_bucket[4].key_hash,
                        test_key_1_same_bucket[4].key,
                        test_key_1_same_bucket[4].key_len,
                        test_value_1 + 4);
                HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_INLINE(
                        chain_ring3,
                        5,
                        test_key_1_same_bucket[5].key_hash,
                        test_key_1_same_bucket[5].key,
                        test_key_1_same_bucket[5].key_len,
                        test_value_1 + 5);

                REQUIRE(hashtable_op_get(
                        hashtable,
                        (char*)test_key_1_same_bucket[1].key,
                        test_key_1_same_bucket[1].key_len,
                        &value));
                REQUIRE(value == test_value_1 + 1);

                REQUIRE(hashtable_op_get(
                        hashtable,
                        (char*)test_key_1_same_bucket[2].key,
                        test_key_1_same_bucket[2].key_len,
                        &value));
                REQUIRE(value == test_value_1 + 2);

                REQUIRE(hashtable_op_get(
                        hashtable,
                        (char*)test_key_1_same_bucket[3].key,
                        test_key_1_same_bucket[3].key_len,
                        &value));
                REQUIRE(value == test_value_1 + 3);

                REQUIRE(hashtable_op_get(
                        hashtable,
                        (char*)test_key_1_same_bucket[4].key,
                        test_key_1_same_bucket[4].key_len,
                        &value));
                REQUIRE(value == test_value_1 + 4);

                REQUIRE(hashtable_op_get(
                        hashtable,
                        (char*)test_key_1_same_bucket[5].key,
                        test_key_1_same_bucket[5].key_len,
                        &value));
                REQUIRE(value == test_value_1 + 5);
            })
        }

        SECTION("not found - test deleted flag") {
            HASHTABLE(buckets_initial_count_5, false, {
                hashtable_bucket_chain_ring_t* chain_ring = HASHTABLE_BUCKET_CHAIN_RING_NEW();
                hashtable->ht_current->buckets[test_index_1_buckets_count_42].chain_first_ring = chain_ring;

                HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_INLINE(
                        chain_ring,
                        1,
                        test_key_1_hash,
                        test_key_1,
                        test_key_1_len,
                        test_value_1);

                HASHTABLE_BUCKET_KEY_VALUE_SET_FLAG(
                        chain_ring->keys_values[1].flags,
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
                hashtable_bucket_chain_ring_t* chain_ring = HASHTABLE_BUCKET_CHAIN_RING_NEW();
                hashtable->ht_current->buckets[test_index_1_buckets_count_42].chain_first_ring = chain_ring;

                HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_INLINE(
                        chain_ring,
                        1,
                        test_key_1_hash,
                        test_key_1,
                        test_key_1_len,
                        test_value_1);

                // Unset the flags and the data to simulate finding a value in the process of being set
                chain_ring->keys_values[1].flags = 0;
                chain_ring->keys_values[1].data = 0;

                REQUIRE(!hashtable_op_get(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        &value));
            })
        }

        SECTION("found - single bucket - get key after delete with hash still in hashes (edge case because of parallelism)") {
            HASHTABLE(buckets_initial_count_5, false, {
                hashtable_bucket_chain_ring_t* chain_ring = HASHTABLE_BUCKET_CHAIN_RING_NEW();
                hashtable->ht_current->buckets[test_index_1_buckets_count_42].chain_first_ring = chain_ring;

                HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_INLINE(
                        chain_ring,
                        1,
                        test_key_1_hash,
                        test_key_1,
                        test_key_1_len,
                        test_value_1);

                HASHTABLE_BUCKET_KEY_VALUE_SET_FLAG(
                        chain_ring->keys_values[1].flags,
                        HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED);

                HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_INLINE(
                        chain_ring,
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
