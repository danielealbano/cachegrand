/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.init
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>
#include <numa.h>

#include "mimalloc.h"
#include "misc.h"
#include "exttypes.h"
#include "xalloc.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "spinlock.h"
#include "intrinsics.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "epoch_gc.h"
#include "data_structures/hashtable_mpmc/hashtable_mpmc.h"

#if CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_T1HA2 == 1
#include "t1ha.h"
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_XXH3 == 1
#include "xxhash.h"
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_CRC32C == 1
#include "hash/hash_crc32c.h"
#else
#error "Unsupported hash algorithm"
#endif

hashtable_mpmc_hash_t test_hashtable_mcmp_support_hash_calculate(
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length) {
#if CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_T1HA2 == 1
    return (hashtable_mpmc_hash_t)t1ha2_atonce(key, key_length, HASHTABLE_MPMC_HASH_SEED);
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_XXH3 == 1
    return (hashtable_mpmc_hash_t)XXH3_64bits_withSeed(key, key_length, HASHTABLE_MPMC_HASH_SEED);
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_CRC32C == 1
    uint32_t crc32 = hash_crc32c(key, key_length, HASHTABLE_MPMC_HASH_SEED);
    hashtable_mpmc_hash_t hash = ((uint64_t)hash_crc32c(key, key_length, crc32) << 32u) | crc32;

    return hash;
#endif
}

#pragma GCC diagnostic ignored "-Wwrite-strings"

TEST_CASE("data_structures/hashtable_mpmc/hashtable_mpmc.c", "[data_structures][hashtable][hashtable_mpmc]") {
    char *key = "This Is A Key - not embedded";
    char *key_different_case = "THIS IS A KEY - NOT EMBEDDED";
    hashtable_mpmc_key_length_t key_length = strlen(key);
    hashtable_mpmc_hash_t key_hash = test_hashtable_mcmp_support_hash_calculate(key, key_length);
    hashtable_mpmc_hash_half_t key_hash_half = key_hash & 0XFFFFFFFF;

    char *key2 = "This Is Another Key - not embedded";
    hashtable_mpmc_key_length_t key2_length = strlen(key2);
    hashtable_mpmc_hash_t key2_hash = test_hashtable_mcmp_support_hash_calculate(key2, key2_length);
    hashtable_mpmc_hash_half_t key2_hash_half = key2_hash & 0XFFFFFFFF;

    char *key_embed = "embedded key";
    hashtable_mpmc_key_length_t key_embed_length = strlen(key_embed);
    hashtable_mpmc_hash_t key_embed_hash = test_hashtable_mcmp_support_hash_calculate(
            key_embed,
            key_embed_length);
    hashtable_mpmc_hash_half_t key_embed_hash_half = key_embed_hash & 0XFFFFFFFF;

    SECTION("hashtable_mpmc_data_init") {
        hashtable_mpmc_data_t *hashtable_data = hashtable_mpmc_data_init(10);

        REQUIRE(hashtable_data != nullptr);
        REQUIRE(hashtable_data->buckets_count == 16);
        REQUIRE(hashtable_data->buckets_count_mask == 16 - 1);
        REQUIRE(hashtable_data->buckets_count_real == 16 + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE);
        REQUIRE(hashtable_data->struct_size ==
                sizeof(hashtable_mpmc_data_t) +
                (sizeof(hashtable_mpmc_bucket_t) * (16 + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE)));

        hashtable_mpmc_data_free(hashtable_data);
    }

    SECTION("hashtable_mpmc_init") {
        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(10, 32, HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

        REQUIRE(hashtable->data != nullptr);
        REQUIRE(hashtable->data->buckets_count == 16);
        REQUIRE(hashtable->data->buckets_count_mask == 16 - 1);
        REQUIRE(hashtable->data->buckets_count_real == 16 + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE);
        REQUIRE(hashtable->data->struct_size ==
                sizeof(hashtable_mpmc_data_t) +
                (sizeof(hashtable_mpmc_bucket_t) * (16 + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE)));
        REQUIRE(hashtable->buckets_count_max == 32);
        REQUIRE(hashtable->upsize_preferred_block_size == HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);
        REQUIRE(hashtable->upsize.from == nullptr);
        REQUIRE(hashtable->upsize.status == HASHTABLE_MPMC_STATUS_NOT_UPSIZING);

        hashtable_mpmc_free(hashtable);
    }

    SECTION("hashtable_mpmc_support_hash_half") {
        REQUIRE(hashtable_mpmc_support_hash_half(key_hash) == key_hash_half);
    }

    SECTION("hashtable_mpmc_support_bucket_index_from_hash") {
        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(10, 32, HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

        REQUIRE(hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_hash) ==
                ((key_hash >> 32) & hashtable->data->buckets_count_mask));

        hashtable_mpmc_free(hashtable);
    }

    SECTION("hashtable_mpmc_support_find_bucket_and_key_value") {
        hashtable_mpmc_bucket_t return_bucket;
        hashtable_mpmc_bucket_index_t return_bucket_index;

        char *key_copy = mi_strdup(key);

        auto key_value = (hashtable_mpmc_data_key_value_t*)xalloc_alloc(sizeof(hashtable_mpmc_data_key_value_t));
        key_value->key.external.key = key_copy;
        key_value->key.external.key_length = key_length;
        key_value->value = 12345;
        key_value->hash = key_hash;
        key_value->key_is_embedded = false;

        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(16, 32, HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);
        hashtable_mpmc_bucket_index_t hashtable_key_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_hash);
        hashtable_mpmc_bucket_index_t hashtable_key_bucket_index_max =
                hashtable_key_bucket_index + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE;
        hashtable_mpmc_bucket_index_t hashtable_key_embed_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_embed_hash);

        SECTION("bucket found") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key,
                    key_length,
                    false,
                    &return_bucket,
                    &return_bucket_index));
            REQUIRE(return_bucket._packed == hashtable->data->buckets[hashtable_key_bucket_index]._packed);
            REQUIRE(return_bucket_index == hashtable_key_bucket_index);
        }

        SECTION("bucket found - temporary") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value =
                    (hashtable_mpmc_data_key_value_volatile_t*)((uintptr_t)(key_value) | 0x01);

            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key,
                    key_length,
                    true,
                    &return_bucket,
                    &return_bucket_index));
            REQUIRE(return_bucket._packed == hashtable->data->buckets[hashtable_key_bucket_index]._packed);
            REQUIRE(return_bucket_index == hashtable_key_bucket_index);
        }

        SECTION("bucket found - embedded key") {
            key_value->key_is_embedded = true;
            strncpy(key_value->key.embedded.key, key_embed, key_embed_length);
            key_value->key.embedded.key_length = key_embed_length;
            key_value->hash = key_embed_hash;
            hashtable->data->buckets[hashtable_key_embed_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_embed_bucket_index].data.hash_half = key_embed_hash_half;
            hashtable->data->buckets[hashtable_key_embed_bucket_index].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key_embed_hash,
                    key_embed_hash_half,
                    key_embed,
                    key_embed_length,
                    false,
                    &return_bucket,
                    &return_bucket_index));
            REQUIRE(return_bucket._packed == hashtable->data->buckets[hashtable_key_embed_bucket_index]._packed);
            REQUIRE(return_bucket_index == hashtable_key_embed_bucket_index);
        }

        SECTION("bucket not found - not existing") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key2_hash,
                    key2_hash_half,
                    key2,
                    key2_length,
                    false,
                    &return_bucket,
                    &return_bucket_index) == false);
        }

        SECTION("bucket not found - temporary") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value =
                    (hashtable_mpmc_data_key_value_volatile_t*)((uintptr_t)(key_value) | 0x01);

            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key,
                    key_length,
                    false,
                    &return_bucket,
                    &return_bucket_index) == false);
        }

        SECTION("bucket not found - not in range") {
            hashtable->data->buckets[hashtable_key_bucket_index_max].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index_max].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index_max].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key,
                    key_length,
                    false,
                    &return_bucket,
                    &return_bucket_index) == false);
        }

        SECTION("bucket not found - hashtable empty") {
            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key,
                    key_length,
                    false,
                    &return_bucket,
                    &return_bucket_index) == false);
        }

        hashtable_mpmc_free(hashtable);
    }

//    SECTION("hashtable_spsc_find_empty_bucket") {
//        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(16);
//        hashtable_mpmc_bucket_index_t hashtable_bucket_index = key_hash & hashtable->data->buckets_count_mask;
//        hashtable_mpmc_bucket_index_t hashtable_bucket_index_max =
//                hashtable_bucket_index + hashtable->max_range;
//
//        SECTION("bucket found") {
//            REQUIRE(hashtable_spsc_find_empty_bucket(
//                    hashtable,
//                    hashtable_bucket_index,
//                    hashtable_bucket_index_max) == hashtable_bucket_index);
//        }
//
//        SECTION("bucket not found - nothing in range") {
//            for (int index = hashtable_bucket_index; index < hashtable_bucket_index_max; index++) {
//                hashtable->hashes[index].set = true;
//            }
//
//            REQUIRE(hashtable_spsc_find_empty_bucket(
//                    hashtable,
//                    hashtable_bucket_index,
//                    hashtable_bucket_index_max) == -1);
//        }
//
//        SECTION("bucket not found - hashtable full") {
//            for (int index = 0; index < hashtable->buckets_count_real; index++) {
//                hashtable->hashes[index].set = true;
//            }
//
//            REQUIRE(hashtable_spsc_find_empty_bucket(
//                    hashtable,
//                    hashtable_bucket_index,
//                    hashtable_bucket_index_max) == -1);
//        }
//
//        hashtable_mpmc_free(hashtable);
//    }

    SECTION("hashtable_mpmc_op_set") {
        char *value1 = "first value";
        char *value2 = "second value";
        char *key_copy = mi_strdup(key);
        char *key_copy2 = mi_strdup(key);
        char *key_embed_copy = mi_strdup(key_embed);
        char *key2_copy = mi_strdup(key2);
        bool return_created_new = false;
        bool return_value_updated = false;
        uintptr_t return_previous_value = 0;

        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(16, 32, HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

        hashtable_mpmc_bucket_index_t hashtable_key_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_hash);
        hashtable_mpmc_bucket_index_t hashtable_key2_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key2_hash);
        hashtable_mpmc_bucket_index_t hashtable_key_embed_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_embed_hash);
        hashtable_mpmc_bucket_index_t hashtable_key_bucket_index_max =
                hashtable_key_bucket_index + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE;

        epoch_gc_t *epoch_gc = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE);
        epoch_gc_thread_t *epoch_gc_thread = epoch_gc_thread_init();
        epoch_gc_thread_register_global(epoch_gc, epoch_gc_thread);
        epoch_gc_thread_register_local(epoch_gc_thread);

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_init();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_init();

        SECTION("value set - insert") {
            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key_copy,
                    key_length,
                    (uintptr_t)value1,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);

            REQUIRE(return_created_new);
            REQUIRE(return_value_updated);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index]._packed != 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value != nullptr);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half == key_hash_half);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key_is_embedded == false);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key == key_copy);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key_length ==
                    key_length);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->hash == key_hash);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->value == (uintptr_t)value1);
        }

        SECTION("value set - insert - tombstone") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value =
                    (hashtable_mpmc_data_key_value_t*)HASHTABLE_MPMC_POINTER_TAG_TOMBSTONE;

            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key_copy,
                    key_length,
                    (uintptr_t)value1,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);

            REQUIRE(return_created_new);
            REQUIRE(return_value_updated);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index]._packed != 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value != nullptr);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half == key_hash_half);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key_is_embedded == false);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key == key_copy);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key_length ==
                    key_length);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->hash == key_hash);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->value == (uintptr_t)value1);
        }

        SECTION("value set - insert - embedded key") {
            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key_embed_copy,
                    key_embed_length,
                    (uintptr_t)value1,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);

            REQUIRE(return_created_new);
            REQUIRE(return_value_updated);
            REQUIRE(hashtable->data->buckets[hashtable_key_embed_bucket_index]._packed != 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_embed_bucket_index].data.key_value != nullptr);
            REQUIRE(hashtable->data->buckets[hashtable_key_embed_bucket_index].data.hash_half == key_embed_hash_half);
            REQUIRE(hashtable->data->buckets[hashtable_key_embed_bucket_index].data.key_value->key_is_embedded == true);
            REQUIRE(strncmp(
                    (char*)hashtable->data->buckets[hashtable_key_embed_bucket_index].data.key_value->key.embedded.key,
                    key_embed_copy,
                    key_embed_length) == 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_embed_bucket_index].data.key_value->key.embedded.key_length ==
                    key_embed_length);
            REQUIRE(hashtable->data->buckets[hashtable_key_embed_bucket_index].data.key_value->hash == key_embed_hash);
            REQUIRE(hashtable->data->buckets[hashtable_key_embed_bucket_index].data.key_value->value == (uintptr_t)value1);
        }

        SECTION("value set - update") {
            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key_copy,
                    key_length,
                    (uintptr_t)value1,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);
            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key_copy2,
                    key_length,
                    (uintptr_t)value2,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);

            REQUIRE(!return_created_new);
            REQUIRE(return_value_updated);
            REQUIRE(return_previous_value == (uintptr_t)value1);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index]._packed != 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value != nullptr);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half == key_hash_half);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key_is_embedded == false);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key == key_copy);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key_length ==
                    key_length);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->hash == key_hash);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->value == (uintptr_t)value2);
        }

        SECTION("value set - insert two keys") {
            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key_copy,
                    key_length,
                    (uintptr_t)value1,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);
            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key2_copy,
                    key2_length,
                    (uintptr_t)value2,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);

            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index]._packed != 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value != nullptr);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half == key_hash_half);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key_is_embedded == false);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key == key_copy);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key_length ==
                    key_length);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->hash == key_hash);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->value == (uintptr_t)value1);

            REQUIRE(return_created_new);
            REQUIRE(return_value_updated);
            REQUIRE(hashtable->data->buckets[hashtable_key2_bucket_index]._packed != 0);
            REQUIRE(hashtable->data->buckets[hashtable_key2_bucket_index].data.key_value != nullptr);
            REQUIRE(hashtable->data->buckets[hashtable_key2_bucket_index].data.hash_half == key2_hash_half);
            REQUIRE(hashtable->data->buckets[hashtable_key2_bucket_index].data.key_value->key_is_embedded == false);
            REQUIRE(hashtable->data->buckets[hashtable_key2_bucket_index].data.key_value->key.external.key == key2_copy);
            REQUIRE(hashtable->data->buckets[hashtable_key2_bucket_index].data.key_value->key.external.key_length ==
                    key2_length);
            REQUIRE(hashtable->data->buckets[hashtable_key2_bucket_index].data.key_value->hash == key2_hash);
            REQUIRE(hashtable->data->buckets[hashtable_key2_bucket_index].data.key_value->value == (uintptr_t)value2);
        }

        SECTION("value set - upsize") {
            hashtable_mpmc_data_t *hashtable_mpmc_data_current = hashtable->data;
            for (
                    hashtable_mpmc_bucket_index_t index = hashtable_key_bucket_index;
                    index < hashtable_key_bucket_index_max;
                    index++) {
                hashtable->data->buckets[index].data.hash_half = 12345;
            }

            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key_copy,
                    key_length,
                    (uintptr_t)value1,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);

            // Hashes have to be set back to zero before freeing up the hashtable
            for (
                    hashtable_mpmc_bucket_index_t index = hashtable_key_bucket_index;
                    index < hashtable_key_bucket_index_max;
                    index++) {
                hashtable_mpmc_data_current->buckets[index].data.hash_half = 0;
            }

            // Recalculate the size index as the hashtable has grown
            hashtable_key_bucket_index =
                    hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_hash);

            REQUIRE(hashtable->data->buckets_count_mask == (hashtable_mpmc_data_current->buckets_count_mask << 1) + 1);
            REQUIRE(hashtable->upsize.from != nullptr);
            REQUIRE(hashtable->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING);

            REQUIRE(return_created_new);
            REQUIRE(return_value_updated);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index]._packed != 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value != nullptr);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half == key_hash_half);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key_is_embedded == false);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key == key_copy);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key_length ==
                    key_length);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->hash == key_hash);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->value == (uintptr_t)value1);
        }

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_free();
        hashtable_mpmc_free(hashtable);

        epoch_gc_thread_unregister_local(epoch_gc_thread);
        epoch_gc_thread_unregister_global(epoch_gc_thread);
        epoch_gc_thread_free(epoch_gc_thread);
        epoch_gc_free(epoch_gc);
    }

    SECTION("hashtable_mpmc_op_get") {
        char *key_copy = mi_strdup(key);
        uintptr_t return_value = 0;

        auto key_value = (hashtable_mpmc_data_key_value_t*)xalloc_alloc(sizeof(hashtable_mpmc_data_key_value_t));
        key_value->key.external.key = key_copy;
        key_value->key.external.key_length = key_length;
        key_value->value = 12345;
        key_value->hash = key_hash;
        key_value->key_is_embedded = false;

        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(16, 32, HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

        hashtable_mpmc_bucket_index_t hashtable_key_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_hash);
        hashtable_mpmc_bucket_index_t hashtable_key_embed_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_embed_hash);

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_init();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_init();

        SECTION("value found - existing key") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_op_get(
                    hashtable,
                    key,
                    key_length,
                    &return_value) == HASHTABLE_MPMC_RESULT_TRUE);
            REQUIRE(return_value == 12345);
        }

        SECTION("value found - embedded key") {
            key_value->key_is_embedded = true;
            strncpy(key_value->key.embedded.key, key_embed, key_embed_length);
            key_value->key.embedded.key_length = key_embed_length;
            key_value->hash = key_embed_hash;
            hashtable->data->buckets[hashtable_key_embed_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_embed_bucket_index].data.hash_half = key_embed_hash_half;
            hashtable->data->buckets[hashtable_key_embed_bucket_index].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_op_get(
                    hashtable,
                    key_embed,
                    key_embed_length,
                    &return_value) == HASHTABLE_MPMC_RESULT_TRUE);
            REQUIRE(return_value == 12345);
        }

        SECTION("value found - after tombstone key") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value =
                    (hashtable_mpmc_data_key_value_t*)HASHTABLE_MPMC_POINTER_TAG_TOMBSTONE;
            hashtable->data->buckets[hashtable_key_bucket_index + 1].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index + 1].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index + 1].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_op_get(
                    hashtable,
                    key,
                    key_length,
                    &return_value) == HASHTABLE_MPMC_RESULT_TRUE);
            REQUIRE(return_value == 12345);
        }

        SECTION("value not found - existing key with different case") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_op_get(
                    hashtable,
                    key_different_case,
                    key_length,
                    &return_value) == HASHTABLE_MPMC_RESULT_FALSE);
        }

        SECTION("value not found - non-existent key") {
            REQUIRE(hashtable_mpmc_op_get(
                    hashtable,
                    key,
                    key_length,
                    &return_value) == HASHTABLE_MPMC_RESULT_FALSE);
        }

        SECTION("value not found - temporary") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value =
                    (hashtable_mpmc_data_key_value_volatile_t*)((uintptr_t)(key_value) | 0x01);

            REQUIRE(hashtable_mpmc_op_get(
                    hashtable,
                    key,
                    key_length,
                    &return_value) == HASHTABLE_MPMC_RESULT_FALSE);
        }

        SECTION("value not found - empty (without tombstone) before") {
            hashtable->data->buckets[hashtable_key_bucket_index + 1].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index + 1].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index + 1].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_op_get(
                    hashtable,
                    key,
                    key_length,
                    &return_value) == HASHTABLE_MPMC_RESULT_FALSE);
        }

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_free();
        hashtable_mpmc_free(hashtable);
    }

    SECTION("hashtable_mpmc_op_delete") {
        char *key_copy = mi_strdup(key);

        auto key_value = (hashtable_mpmc_data_key_value_t*)xalloc_alloc(sizeof(hashtable_mpmc_data_key_value_t));
        key_value->key.external.key = key_copy;
        key_value->key.external.key_length = key_length;
        key_value->value = 12345;
        key_value->hash = key_hash;
        key_value->key_is_embedded = false;

        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(16, 32, HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

        hashtable_mpmc_bucket_index_t hashtable_key_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_hash);

        epoch_gc_t *epoch_gc = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE);
        epoch_gc_thread_t *epoch_gc_thread = epoch_gc_thread_init();
        epoch_gc_thread_register_global(epoch_gc, epoch_gc_thread);
        epoch_gc_thread_register_local(epoch_gc_thread);

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_init();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_init();

        SECTION("value deleted - existing key") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_op_delete(hashtable, key, key_length) == HASHTABLE_MPMC_RESULT_TRUE);

            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id == 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half == 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value ==
                    (hashtable_mpmc_data_key_value_t*)HASHTABLE_MPMC_POINTER_TAG_TOMBSTONE);

            epoch_gc_thread_advance_epoch_tsc(epoch_gc_thread);
            REQUIRE(epoch_gc_thread_collect_all(epoch_gc_thread) == 1);
        }

        SECTION("value not deleted - existing key with different case") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_op_delete(
                    hashtable,
                    key_different_case,
                    key_length) == HASHTABLE_MPMC_RESULT_FALSE);

            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index]._packed != 0);
        }

        SECTION("value not deleted - non-existent key") {
            REQUIRE(hashtable_mpmc_op_delete(
                    hashtable,
                    key,
                    key_length) == HASHTABLE_MPMC_RESULT_FALSE);
        }

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_free();
        hashtable_mpmc_free(hashtable);

        epoch_gc_thread_unregister_local(epoch_gc_thread);
        epoch_gc_thread_unregister_global(epoch_gc_thread);
        epoch_gc_thread_free(epoch_gc_thread);
        epoch_gc_free(epoch_gc);
    }
//
//    SECTION("hashtable_spsc_op_iter") {
//        char *value1 = "first value";
//        char *value2 = "second value";
//        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(16);
//        hashtable_spsc_bucket_t *hashtable_buckets = hashtable_spsc_get_buckets(hashtable);
//        hashtable_spsc_bucket_index_t hashtable_bucket_index = 0;
//
//        SECTION("empty hashtable") {
//            REQUIRE(hashtable_spsc_op_iter(hashtable, &hashtable_bucket_index) == NULL);
//            REQUIRE(hashtable_bucket_index == -1);
//        }
//
//        SECTION("hashtable with 1 bucket set") {
//            hashtable->hashes[2].set = true;
//            hashtable_buckets[2].value = value1;
//
//            REQUIRE(hashtable_spsc_op_iter(hashtable, &hashtable_bucket_index) == value1);
//            REQUIRE(hashtable_bucket_index == 2);
//            hashtable_bucket_index++;
//
//            REQUIRE(hashtable_spsc_op_iter(hashtable, &hashtable_bucket_index) == NULL);
//            REQUIRE(hashtable_bucket_index == -1);
//        }
//
//        SECTION("hashtable with 2 bucket set") {
//            hashtable->hashes[2].set = true;
//            hashtable_buckets[2].value = value1;
//            hashtable->hashes[6].set = true;
//            hashtable_buckets[6].value = value2;
//
//            REQUIRE(hashtable_spsc_op_iter(hashtable, &hashtable_bucket_index) == value1);
//            REQUIRE(hashtable_bucket_index == 2);
//            hashtable_bucket_index++;
//
//            REQUIRE(hashtable_spsc_op_iter(hashtable, &hashtable_bucket_index) == value2);
//            REQUIRE(hashtable_bucket_index == 6);
//            hashtable_bucket_index++;
//
//            REQUIRE(hashtable_spsc_op_iter(hashtable, &hashtable_bucket_index) == NULL);
//            REQUIRE(hashtable_bucket_index == -1);
//        }
//
//        hashtable_mpmc_free(hashtable);
//    }
}
