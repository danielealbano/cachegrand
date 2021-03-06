#include <catch2/catch.hpp>
#include <numa.h>

#include <string.h>

#include "exttypes.h"
#include "spinlock.h"
#include "random.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"
#include "data_structures/hashtable/mcmp/hashtable_support_index.h"
#include "data_structures/hashtable/mcmp/hashtable_op_set.h"
#include "data_structures/hashtable/mcmp/hashtable_op_delete.h"

#include "../support.h"
#include "fixtures-hashtable.h"

TEST_CASE("hashtable/hashtable_mcmp_op_delete.c", "[hashtable][hashtable_op][hashtable_mcmp_op_delete]") {
    SECTION("hashtable_mcmp_op_delete") {
        SECTION("delete non-existing") {
            HASHTABLE(0x7FFF, false, {
                REQUIRE(!hashtable_mcmp_op_delete(
                        hashtable,
                        test_key_1,
                        test_key_1_len));
            })
        }

        SECTION("set and delete 1 bucket") {
            HASHTABLE(0x7FFF, false, {
                hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(hashtable_mcmp_support_index_from_hash(
                        hashtable->ht_current->buckets_count,
                        test_key_1_hash));
                hashtable_chunk_slot_index_t chunk_slot_index = 0;

                hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk =
                        &hashtable->ht_current->half_hashes_chunk[chunk_index];
                hashtable_key_value_volatile_t *key_value =
                        &hashtable->ht_current->keys_values[HASHTABLE_TO_BUCKET_INDEX(chunk_index, chunk_slot_index)];

                REQUIRE(hashtable_mcmp_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_1));

                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].quarter_hash == test_key_1_hash_quarter);
                REQUIRE(key_value->flags != HASHTABLE_KEY_VALUE_FLAG_DELETED);

                REQUIRE(hashtable_mcmp_op_delete(
                        hashtable,
                        test_key_1,
                        test_key_1_len));

                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].slot_id == 0);
                REQUIRE(key_value->flags == HASHTABLE_KEY_VALUE_FLAG_DELETED);
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

                REQUIRE(hashtable_mcmp_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_1));

                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].quarter_hash == test_key_1_hash_quarter);
                REQUIRE(key_value->flags != HASHTABLE_KEY_VALUE_FLAG_DELETED);

                REQUIRE(hashtable_mcmp_op_delete(
                        hashtable,
                        test_key_1,
                        test_key_1_len));

                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].slot_id == 0);
                REQUIRE(key_value->flags == HASHTABLE_KEY_VALUE_FLAG_DELETED);

                REQUIRE(hashtable_mcmp_op_set(
                        hashtable,
                        test_key_1,
                        test_key_1_len,
                        test_value_1));

                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].filled == true);
                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].distance == 0);
                REQUIRE(half_hashes_chunk->half_hashes[chunk_slot_index].quarter_hash == test_key_1_hash_quarter);
                REQUIRE(key_value->flags != HASHTABLE_KEY_VALUE_FLAG_DELETED);

                REQUIRE(hashtable_mcmp_op_delete(
                        hashtable,
                        test_key_1,
                        test_key_1_len));

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
                    REQUIRE(hashtable_mcmp_op_set(
                            hashtable,
                            (char *) test_key_same_bucket[i].key,
                            test_key_same_bucket[i].key_len,
                            test_value_1 + i));
                }

                hashtable_chunk_slot_index_t random_slot_index = random_generate() % slots_to_fill;

                REQUIRE(hashtable_mcmp_op_delete(
                        hashtable,
                        test_key_same_bucket[random_slot_index].key,
                        test_key_same_bucket[random_slot_index].key_len));

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

                test_support_same_hash_mod_fixtures_free(test_key_same_bucket);
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
                    REQUIRE(hashtable_mcmp_op_set(
                            hashtable,
                            (char *) test_key_same_bucket[i].key,
                            test_key_same_bucket[i].key_len,
                            test_value_1 + i));
                }

                hashtable_chunk_slot_index_t random_slot_index = random_generate() % (slots_to_fill - 1);

                REQUIRE(hashtable_mcmp_op_delete(
                        hashtable,
                        test_key_same_bucket[random_slot_index].key,
                        test_key_same_bucket[random_slot_index].key_len));

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

                REQUIRE(hashtable_mcmp_op_set(
                        hashtable,
                        (char *) test_key_same_bucket[slots_to_fill - 1].key,
                        test_key_same_bucket[slots_to_fill - 1].key_len,
                        test_value_1 + slots_to_fill - 1));

                REQUIRE(half_hashes_chunk->half_hashes[random_slot_index].filled == true);
                REQUIRE(half_hashes_chunk->half_hashes[random_slot_index].distance == 0);
                REQUIRE(half_hashes_chunk->half_hashes[random_slot_index].quarter_hash ==
                        test_key_same_bucket[slots_to_fill - 1].key_hash_quarter);
                REQUIRE(key_value->flags == HASHTABLE_KEY_VALUE_FLAG_FILLED);
                REQUIRE(key_value->data == test_value_1 + slots_to_fill - 1);

                test_support_same_hash_mod_fixtures_free(test_key_same_bucket);
            })
        }
    }
}
