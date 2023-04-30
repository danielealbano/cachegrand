/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "exttypes.h"
#include "misc.h"
#include "xalloc.h"
#include "pow2.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"

#include "hashtable_spsc.h"

#define TAG "hashtable_spsc"

hashtable_spsc_t *hashtable_spsc_new(
        hashtable_spsc_bucket_count_t buckets_count,
        uint16_t max_range,
        bool free_keys_on_deallocation) {
    hashtable_spsc_bucket_count_t buckets_count_pow2_next, buckets_to_allocate;

    assert(buckets_count > 0);
    assert(max_range > 0);
    assert(buckets_count < UINT32_MAX);

    buckets_count_pow2_next = (uint32_t)pow2_next(buckets_count);
    buckets_to_allocate = buckets_count_pow2_next + max_range;

    assert(buckets_count_pow2_next >= buckets_count);
    assert(buckets_to_allocate >= buckets_count_pow2_next);

    size_t hashtable_size =
            sizeof(hashtable_spsc_t) +
            (sizeof(hashtable_spsc_hash_t) * buckets_to_allocate) +
            (sizeof(hashtable_spsc_bucket_t) * buckets_to_allocate);
    hashtable_spsc_t *hashtable = xalloc_alloc_zero(hashtable_size);

    hashtable->buckets_count = buckets_count;
    hashtable->buckets_count_pow2 = buckets_count_pow2_next;
    hashtable->buckets_count_real = buckets_to_allocate;
    hashtable->max_range = max_range;
    hashtable->free_keys_on_deallocation = free_keys_on_deallocation;

    return hashtable;
}

void hashtable_spsc_free(
        hashtable_spsc_t *hashtable) {
    if (hashtable->free_keys_on_deallocation) {
        hashtable_spsc_bucket_t *buckets = hashtable_spsc_get_buckets(hashtable);
        for(
                hashtable_spsc_bucket_index_t bucket_index = 0;
                bucket_index < hashtable->buckets_count_real;
                bucket_index++) {
            if (!hashtable->hashes[bucket_index].set) {
                continue;
            }

            ffma_mem_free((void*)buckets[bucket_index].key);
        }
    }

    xalloc_free(hashtable);
}

hashtable_spsc_t* hashtable_spsc_upsize(
        hashtable_spsc_t *hashtable_current) {
    // Creates a new hashtable with the same parameters as the original one but twice the buckets
    hashtable_spsc_t *hashtable_uspized = hashtable_spsc_new(
            hashtable_current->buckets_count * 2,
            hashtable_current->max_range,
            hashtable_current->free_keys_on_deallocation);
    hashtable_spsc_bucket_t *hashtable_uspized_buckets = hashtable_spsc_get_buckets(hashtable_uspized);

    // Iterate over the original hashtable and copy the values to the new one
    hashtable_spsc_bucket_t *hashtable_current_buckets = hashtable_spsc_get_buckets(hashtable_current);
    for(
            hashtable_spsc_bucket_index_t bucket_index_current = 0;
            bucket_index_current < hashtable_current->buckets_count_real;
            bucket_index_current++) {
        if (!hashtable_current->hashes[bucket_index_current].set) {
            continue;
        }

        // Get the hash and calculate the new bucket index
        hashtable_spsc_cmp_hash_t hash = hashtable_current->hashes[bucket_index_current].cmp_hash;
        hashtable_spsc_bucket_index_t bucket_index_uspized =
                hashtable_spsc_bucket_index_from_hash(hashtable_uspized, hash);
        hashtable_spsc_bucket_index_t bucket_index_max_uspized = bucket_index_uspized + hashtable_uspized->max_range;
        bucket_index_uspized = hashtable_spsc_find_empty_bucket(
                hashtable_uspized,
                bucket_index_uspized,
                bucket_index_max_uspized);

        if (unlikely(bucket_index_uspized == -1)) {
            FATAL(TAG, "Unable to find an empty bucket in the new hashtable during upsize");
        }

        hashtable_uspized->hashes[bucket_index_uspized].set = true;
        hashtable_uspized->hashes[bucket_index_uspized].cmp_hash = hash;
        hashtable_uspized_buckets[bucket_index_uspized].key =
                hashtable_current_buckets[bucket_index_current].key;
        hashtable_uspized_buckets[bucket_index_uspized].key_length =
                hashtable_current_buckets[bucket_index_current].key_length;
        hashtable_uspized_buckets[bucket_index_uspized].value =
                hashtable_current_buckets[bucket_index_current].value;
    }

    // Disable the free of the keys
    hashtable_current->free_keys_on_deallocation = false;

    // Free the original hashtable
    hashtable_spsc_free(hashtable_current);

    // Return the new hashtable
    return hashtable_uspized;
}

bool hashtable_spsc_op_delete_by_bucket_index(
        hashtable_spsc_t *hashtable,
        hashtable_spsc_bucket_index_t bucket_index) {
    hashtable->hashes[bucket_index].set = false;

    if (hashtable->free_keys_on_deallocation) {
        hashtable_spsc_bucket_t *buckets = hashtable_spsc_get_buckets(hashtable);
        ffma_mem_free((void*)buckets[bucket_index].key);
    }

    return true;
}

bool hashtable_spsc_op_try_set_ci(
        hashtable_spsc_t *hashtable,
        const char *key,
        hashtable_spsc_key_length_t key_length,
        void *value) {
    hashtable_spsc_bucket_index_t bucket_index;
    hashtable_spsc_bucket_count_t bucket_index_max;

    uint32_t hash = fnv_32_hash_ci((void *)key, key_length);

    // Search if there is already a bucket with the same hash and key
    bucket_index = hashtable_spsc_find_bucket_index_by_key_ci(hashtable, hash, key, key_length);

    if (bucket_index == -1) {
        // If not search for an empty bucket within the allowed range
        bucket_index = hashtable_spsc_bucket_index_from_hash(hashtable, hash);
        bucket_index_max = bucket_index + hashtable->max_range;
        bucket_index = hashtable_spsc_find_empty_bucket(
                hashtable,
                bucket_index,
                bucket_index_max);

        if (bucket_index > -1) {
            // If an empty bucket was found, update the hash and mark it as in use
            hashtable->hashes[bucket_index].set = true;
            hashtable->hashes[bucket_index].cmp_hash = HASHTABLE_SPSC_HASH(hash);
        }
    }

    if (unlikely(bucket_index == -1)) {
        return false;
    }

    hashtable_spsc_bucket_t *buckets = hashtable_spsc_get_buckets(hashtable);

    buckets[bucket_index].key = key;
    buckets[bucket_index].key_length = key_length;
    buckets[bucket_index].value = value;

    return true;
}

bool hashtable_spsc_op_delete_ci(
        hashtable_spsc_t *hashtable,
        const char* key,
        hashtable_spsc_key_length_t key_length) {
    uint32_t hash = fnv_32_hash_ci((void *)key, key_length);
    hashtable_spsc_bucket_index_t bucket_index = hashtable_spsc_find_bucket_index_by_key_ci(
            hashtable,
            hash,
            key,
            key_length);

    if (unlikely(bucket_index == -1)) {
        return false;
    }

    hashtable_spsc_op_delete_by_bucket_index(hashtable, bucket_index);
    return true;
}

bool hashtable_spsc_op_try_set_cs(
        hashtable_spsc_t *hashtable,
        const char *key,
        hashtable_spsc_key_length_t key_length,
        void *value) {
    hashtable_spsc_bucket_index_t bucket_index;
    hashtable_spsc_bucket_count_t bucket_index_max;

    uint32_t hash = fnv_32_hash((void *)key, key_length);

    // Search if there is already a bucket with the same hash and key
    bucket_index = hashtable_spsc_find_bucket_index_by_key_cs(hashtable, hash, key, key_length);

    if (bucket_index == -1) {
        // If not search for an empty bucket within the allowed range
        bucket_index = hashtable_spsc_bucket_index_from_hash(hashtable, hash);
        bucket_index_max = bucket_index + hashtable->max_range;
        bucket_index = hashtable_spsc_find_empty_bucket(
                hashtable,
                bucket_index,
                bucket_index_max);

        if (bucket_index > -1) {
            // If an empty bucket was found, update the hash and mark it as in use
            hashtable->hashes[bucket_index].set = true;
            hashtable->hashes[bucket_index].cmp_hash = HASHTABLE_SPSC_HASH(hash);
        }
    }

    if (unlikely(bucket_index == -1)) {
        return false;
    }

    hashtable_spsc_bucket_t *buckets = hashtable_spsc_get_buckets(hashtable);

    buckets[bucket_index].key = key;
    buckets[bucket_index].key_length = key_length;
    buckets[bucket_index].value = value;

    return true;
}

bool hashtable_spsc_op_delete_cs(
        hashtable_spsc_t *hashtable,
        const char* key,
        hashtable_spsc_key_length_t key_length) {
    uint32_t hash = fnv_32_hash((void *)key, key_length);
    hashtable_spsc_bucket_index_t bucket_index = hashtable_spsc_find_bucket_index_by_key_cs(
            hashtable,
            hash,
            key,
            key_length);

    if (unlikely(bucket_index == -1)) {
        return false;
    }

    hashtable_spsc_op_delete_by_bucket_index(hashtable, bucket_index);
    return true;
}

bool hashtable_spsc_op_try_set_by_hash(
        hashtable_spsc_t *hashtable,
        uint32_t hash,
        const char *key,
        hashtable_spsc_key_length_t key_length,
        void *value) {
    hashtable_spsc_bucket_index_t bucket_index;
    hashtable_spsc_bucket_count_t bucket_index_max;

    // Search if there is already a bucket with the same hash and key
    bucket_index = hashtable_spsc_find_bucket_index_by_key_cs(hashtable, hash, key, key_length);

    if (bucket_index == -1) {
        // If not search for an empty bucket within the allowed range
        bucket_index = hashtable_spsc_bucket_index_from_hash(hashtable, hash);
        bucket_index_max = bucket_index + hashtable->max_range;
        bucket_index = hashtable_spsc_find_empty_bucket(
                hashtable,
                bucket_index,
                bucket_index_max);

        if (bucket_index > -1) {
            // If an empty bucket was found, update the hash and mark it as in use
            hashtable->hashes[bucket_index].set = true;
            hashtable->hashes[bucket_index].cmp_hash = HASHTABLE_SPSC_HASH(hash);
        }
    }

    if (unlikely(bucket_index == -1)) {
        return false;
    }

    hashtable_spsc_bucket_t *buckets = hashtable_spsc_get_buckets(hashtable);

    buckets[bucket_index].key = key;
    buckets[bucket_index].key_length = key_length;
    buckets[bucket_index].value = value;

    return true;
}

bool hashtable_spsc_op_delete_by_hash(
        hashtable_spsc_t *hashtable,
        uint32_t hash,
        const char* key,
        hashtable_spsc_key_length_t key_length) {
    hashtable_spsc_bucket_index_t bucket_index = hashtable_spsc_find_bucket_index_by_key_cs(
            hashtable,
            hash,
            key,
            key_length);

    if (unlikely(bucket_index == -1)) {
        return false;
    }

    hashtable_spsc_op_delete_by_bucket_index(hashtable, bucket_index);
    return true;
}


bool hashtable_spsc_op_try_set_by_hash_and_key_uint32(
        hashtable_spsc_t *hashtable,
        uint32_t hash,
        const uint32_t key_uint32,
        void *value) {
    hashtable_spsc_bucket_index_t bucket_index;
    hashtable_spsc_bucket_count_t bucket_index_max;

    assert(hashtable->free_keys_on_deallocation == false);

    // Search if there is already a bucket with the same hash and key
    bucket_index = hashtable_spsc_find_bucket_index_by_key_uint32(hashtable, hash, key_uint32);

    if (bucket_index == -1) {
        // If not search for an empty bucket within the allowed range
        bucket_index = hashtable_spsc_bucket_index_from_hash(hashtable, hash);
        bucket_index_max = bucket_index + hashtable->max_range;
        bucket_index = hashtable_spsc_find_empty_bucket(
                hashtable,
                bucket_index,
                bucket_index_max);

        if (bucket_index > -1) {
            // If an empty bucket was found, update the hash and mark it as in use
            hashtable->hashes[bucket_index].set = true;
            hashtable->hashes[bucket_index].cmp_hash = HASHTABLE_SPSC_HASH(hash);
        }
    }

    if (unlikely(bucket_index == -1)) {
        return false;
    }

    hashtable_spsc_bucket_t *buckets = hashtable_spsc_get_buckets(hashtable);

    buckets[bucket_index].key_uint32 = key_uint32;
    buckets[bucket_index].key_length = 0;
    buckets[bucket_index].value = value;

    return true;
}

void *hashtable_spsc_op_iter(
        hashtable_spsc_t *hashtable,
        hashtable_spsc_bucket_index_t *bucket_index) {
    *bucket_index = hashtable_spsc_find_set_bucket(
            hashtable,
            *bucket_index,
            hashtable->buckets_count_real);

    if (unlikely(*bucket_index == -1)) {
        return NULL;
    }

    hashtable_spsc_bucket_t *buckets = hashtable_spsc_get_buckets(hashtable);
    return buckets[*bucket_index].value;
}
