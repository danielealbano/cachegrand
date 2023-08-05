/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "config.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "memory_allocator/ffma.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/storage.h"
#include "storage/db/storage_db.h"

#include "storage_db_hashset.h"

void storage_db_hashset_free(
        storage_db_hashset_t *hashtable) {
    if (hashtable->free_keys_on_deallocation) {
        storage_db_hashset_bucket_t *buckets = storage_db_hashset_get_buckets(hashtable);
        for(
                storage_db_hashset_bucket_index_t bucket_index = 0;
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

storage_db_hashset_t* storage_db_hashset_upsize(
        storage_db_hashset_t *hashtable_current) {
    // Creates a new hashtable with the same parameters as the original one but twice the buckets
    storage_db_hashset_t *hashtable_uspized = storage_db_hashset_new(
            hashtable_current->buckets_count * 2,
            hashtable_current->max_range,
            hashtable_current->free_keys_on_deallocation);
    storage_db_hashset_bucket_t *hashtable_uspized_buckets = storage_db_hashset_get_buckets(hashtable_uspized);

    // Iterate over the original hashtable and copy the values to the new one
    storage_db_hashset_bucket_t *hashtable_current_buckets = storage_db_hashset_get_buckets(hashtable_current);
    for(
            storage_db_hashset_bucket_index_t bucket_index_current = 0;
            bucket_index_current < hashtable_current->buckets_count_real;
            bucket_index_current++) {
        if (!hashtable_current->hashes[bucket_index_current].set) {
            continue;
        }

        // Get the hash and calculate the new bucket index
        storage_db_hashset_cmp_hash_t hash = hashtable_current->hashes[bucket_index_current].cmp_hash;
        storage_db_hashset_bucket_index_t bucket_index_uspized =
                storage_db_hashset_bucket_index_from_hash(hashtable_uspized, hash);
        storage_db_hashset_bucket_index_t bucket_index_max_uspized = bucket_index_uspized + hashtable_uspized->max_range;
        bucket_index_uspized = storage_db_hashset_find_empty_bucket(
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
    storage_db_hashset_free(hashtable_current);

    // Return the new hashtable
    return hashtable_uspized;
}

bool storage_db_hashset_op_delete_by_bucket_index(
        storage_db_hashset_t *hashtable,
        storage_db_hashset_bucket_index_t bucket_index) {
    hashtable->hashes[bucket_index].set = false;

    if (hashtable->free_keys_on_deallocation) {
        storage_db_hashset_bucket_t *buckets = storage_db_hashset_get_buckets(hashtable);
        ffma_mem_free((void*)buckets[bucket_index].key);
    }

    return true;
}

bool storage_db_hashset_op_try_set_cs(
        storage_db_hashset_t *hashtable,
        const char *key,
        storage_db_hashset_key_length_t key_length,
        void *value) {
    storage_db_hashset_bucket_index_t bucket_index;
    storage_db_hashset_bucket_count_t bucket_index_max;

    uint32_t hash = fnv_32_hash((void *)key, key_length);

    // Search if there is already a bucket with the same hash and key
    bucket_index = storage_db_hashset_find_bucket_index_by_key_cs(hashtable, hash, key, key_length);

    if (bucket_index == -1) {
        // If not search for an empty bucket within the allowed range
        bucket_index = storage_db_hashset_bucket_index_from_hash(hashtable, hash);
        bucket_index_max = bucket_index + hashtable->max_range;
        bucket_index = storage_db_hashset_find_empty_bucket(
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

    storage_db_hashset_bucket_t *buckets = storage_db_hashset_get_buckets(hashtable);

    buckets[bucket_index].key = key;
    buckets[bucket_index].key_length = key_length;
    buckets[bucket_index].value = value;

    return true;
}

bool storage_db_hashset_op_delete_cs(
        storage_db_hashset_t *hashtable,
        const char* key,
        storage_db_hashset_key_length_t key_length) {
    uint32_t hash = fnv_32_hash((void *)key, key_length);
    storage_db_hashset_bucket_index_t bucket_index = storage_db_hashset_find_bucket_index_by_key_cs(
            hashtable,
            hash,
            key,
            key_length);

    if (unlikely(bucket_index == -1)) {
        return false;
    }

    storage_db_hashset_op_delete_by_bucket_index(hashtable, bucket_index);
    return true;
}

bool storage_db_hashset_op_try_set_by_hash(
        storage_db_hashset_t *hashtable,
        uint32_t hash,
        const char *key,
        storage_db_hashset_key_length_t key_length,
        void *value) {
    storage_db_hashset_bucket_index_t bucket_index;
    storage_db_hashset_bucket_count_t bucket_index_max;

    // Search if there is already a bucket with the same hash and key
    bucket_index = storage_db_hashset_find_bucket_index_by_key_cs(hashtable, hash, key, key_length);

    if (bucket_index == -1) {
        // If not search for an empty bucket within the allowed range
        bucket_index = storage_db_hashset_bucket_index_from_hash(hashtable, hash);
        bucket_index_max = bucket_index + hashtable->max_range;
        bucket_index = storage_db_hashset_find_empty_bucket(
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

    storage_db_hashset_bucket_t *buckets = storage_db_hashset_get_buckets(hashtable);

    buckets[bucket_index].key = key;
    buckets[bucket_index].key_length = key_length;
    buckets[bucket_index].value = value;

    return true;
}

bool storage_db_hashset_op_delete_by_hash(
        storage_db_hashset_t *hashtable,
        uint32_t hash,
        const char* key,
        storage_db_hashset_key_length_t key_length) {
    storage_db_hashset_bucket_index_t bucket_index = storage_db_hashset_find_bucket_index_by_key_cs(
            hashtable,
            hash,
            key,
            key_length);

    if (unlikely(bucket_index == -1)) {
        return false;
    }

    storage_db_hashset_op_delete_by_bucket_index(hashtable, bucket_index);
    return true;
}

void *storage_db_hashset_op_iter(
        storage_db_hashset_t *hashtable,
        storage_db_hashset_bucket_index_t *bucket_index) {
    *bucket_index = storage_db_hashset_find_set_bucket(
            hashtable,
            *bucket_index,
            hashtable->buckets_count_real);

    if (unlikely(*bucket_index == -1)) {
        return NULL;
    }

    storage_db_hashset_bucket_t *buckets = storage_db_hashset_get_buckets(hashtable);
    return buckets[*bucket_index].value;
}
