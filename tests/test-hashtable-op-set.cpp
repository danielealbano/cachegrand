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

                volatile hashtable_bucket_t* bucket =
                        &hashtable->ht_current->buckets[bucket_index];

                volatile hashtable_bucket_chain_ring_t* chain_ring = bucket->chain_first_ring;

                // Check if the write lock has been released
                REQUIRE(bucket->write_lock == 0);

                // Check if the first slot of the chain ring contains the correct key/value
                REQUIRE(chain_ring->half_hashes[0] == test_key_1_hash_half);
                REQUIRE(chain_ring->keys_values[0].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)chain_ring->keys_values[0].inline_key.data,
                        test_key_1,
                        test_key_1_len) == 0);
                REQUIRE(chain_ring->keys_values[0].data == test_value_1);

                // Check if the subsequent element has been affected by the changes
                REQUIRE(chain_ring->half_hashes[1] == 0);
                REQUIRE(chain_ring->keys_values[1].flags == 0);
                REQUIRE(chain_ring->keys_values[1].inline_key.data[0] == 0);
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
                        test_value_2));

                hashtable_bucket_index_t bucket_index = hashtable_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash);

                volatile hashtable_bucket_t* bucket =
                        &hashtable->ht_current->buckets[bucket_index];

                volatile hashtable_bucket_chain_ring_t* chain_ring = bucket->chain_first_ring;

                // Check if the first slot of the chain ring contains the correct key/value
                REQUIRE(chain_ring->half_hashes[0] == test_key_1_hash_half);
                REQUIRE(chain_ring->keys_values[0].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)chain_ring->keys_values[0].inline_key.data,
                        test_key_1,
                        test_key_1_len) == 0);
                REQUIRE(chain_ring->keys_values[0].data == test_value_2);

                // Check if the subsequent element has been affected by the changes
                REQUIRE(chain_ring->half_hashes[1] == 0);
                REQUIRE(chain_ring->keys_values[1].flags == 0);
                REQUIRE(chain_ring->keys_values[1].inline_key.data[0] == 0);
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

                volatile hashtable_bucket_chain_ring_t* chain_ring1 =
                        hashtable->ht_current->buckets[bucket_index1].chain_first_ring;

                hashtable_bucket_index_t bucket_index2 = hashtable_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_2_hash);

                volatile hashtable_bucket_chain_ring_t* chain_ring2 =
                        hashtable->ht_current->buckets[bucket_index2].chain_first_ring;

                // Check if the first slot of the chain ring contains the correct key/value
                REQUIRE(chain_ring1->half_hashes[0] == test_key_1_hash_half);
                REQUIRE(chain_ring1->keys_values[0].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)chain_ring1->keys_values[0].inline_key.data,
                        test_key_1,
                        test_key_1_len) == 0);
                REQUIRE(chain_ring1->keys_values[0].data == test_value_1);

                // Check if the first slot of the chain ring contains the correct key/value
                REQUIRE(chain_ring2->half_hashes[0] == test_key_2_hash_half);
                REQUIRE(chain_ring2->keys_values[0].flags ==
                        (HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE));
                REQUIRE(strncmp(
                        (char*)chain_ring2->keys_values[0].inline_key.data,
                        test_key_2,
                        test_key_2_len) == 0);
                REQUIRE(chain_ring2->keys_values[0].data == test_value_2);
            })
        }

        SECTION("fill multiple chain rings - key with same prefix - key not inline") {
            HASHTABLE(buckets_initial_count_5, false, {
                uint32_t slots_to_fill = (HASHTABLE_BUCKET_CHAIN_RING_SLOTS_COUNT * 3) + 3;

                for(uint32_t i = 0; i < slots_to_fill; i++) {
                    REQUIRE(hashtable_op_set(
                            hashtable,
                            (char*)test_key_1_same_bucket[i].key,
                            test_key_1_same_bucket[i].key_len,
                            test_value_1 + i));
                }

                volatile hashtable_bucket_chain_ring_t* chain_ring =
                        hashtable->ht_current->buckets[test_index_1_buckets_count_42].chain_first_ring;

                for(uint32_t i = 0; i < slots_to_fill; i++) {
                    uint8_t slot_index = i % 8;
                    if (slot_index == 0 && i > 0) {
                        chain_ring = chain_ring->next_ring;
                    }

                    REQUIRE(chain_ring->half_hashes[slot_index] == test_key_1_same_bucket[i].key_hash_half);
                    REQUIRE(chain_ring->keys_values[slot_index].flags == HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED);
                    REQUIRE(strncmp(
                            (char*)chain_ring->keys_values[slot_index].prefix_key.data,
                            test_key_1_same_bucket[i].key,
                            HASHTABLE_KEY_PREFIX_SIZE) == 0);
                    REQUIRE(chain_ring->keys_values[slot_index].data == test_value_1 + i);
                }
            })
        }
    }
}
