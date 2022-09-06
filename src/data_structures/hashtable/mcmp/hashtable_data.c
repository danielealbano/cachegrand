/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <numa.h>
#include <assert.h>

#include "exttypes.h"
#include "spinlock.h"
#include "xalloc.h"
#include "pow2.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/fast_fixed_memory_allocator.h"

#include "hashtable.h"
#include "hashtable_data.h"
#include "hashtable_thread_counters.h"

hashtable_data_t* hashtable_mcmp_data_init(
        hashtable_bucket_count_t buckets_count) {
    if (pow2_is(buckets_count) == false) {
        return NULL;
    }

    hashtable_data_t* hashtable_data = (hashtable_data_t*)xalloc_alloc(sizeof(hashtable_data_t));

    hashtable_data->buckets_count =
            buckets_count;
    hashtable_data->buckets_count_real =
            hashtable_data->buckets_count -
            (buckets_count % HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT) +
            HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT +
            (HASHTABLE_HALF_HASHES_CHUNK_SEARCH_MAX * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);
    hashtable_data->chunks_count =
            hashtable_data->buckets_count_real / HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;

    assert(hashtable_data->chunks_count * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT == hashtable_data->buckets_count_real);

    hashtable_data->half_hashes_chunk_size =
            sizeof(hashtable_half_hashes_chunk_volatile_t) * hashtable_data->chunks_count;
    hashtable_data->keys_values_size =
            sizeof(hashtable_key_value_volatile_t) * hashtable_data->buckets_count_real;

    hashtable_data->half_hashes_chunk =
            (hashtable_half_hashes_chunk_volatile_t *)xalloc_mmap_alloc(hashtable_data->half_hashes_chunk_size);
    hashtable_data->keys_values =
            (hashtable_key_value_volatile_t *)xalloc_mmap_alloc(hashtable_data->keys_values_size);

    hashtable_mcmp_thread_counters_init(hashtable_data, 0);

    return hashtable_data;
}

bool hashtable_mcmp_data_numa_interleave_memory(
        hashtable_data_t* hashtable_data,
        struct bitmask* numa_nodes_bitmask) {
    // Can't use numa_interleave_memory with only one numa node so if it's requested fail
    if (numa_available() < 0 || numa_num_configured_nodes() < 2) {
        return false;
    }

    numa_interleave_memory(
            (void*)hashtable_data->half_hashes_chunk,
            hashtable_data->half_hashes_chunk_size,
            numa_nodes_bitmask);

    if (errno != 0) {
        return false;
    }

    numa_interleave_memory(
            (void*)hashtable_data->keys_values,
            hashtable_data->keys_values_size,
            numa_nodes_bitmask);

    if (errno != 0) {
        return false;
    }

    return true;
}

void hashtable_mcmp_data_keys_free(
        hashtable_data_t* hashtable_data) {
    for (
            hashtable_bucket_index_t bucket_index = 0;
            bucket_index < hashtable_data->buckets_count_real;
            bucket_index++) {
        hashtable_key_value_volatile_t *key_value = &hashtable_data->keys_values[bucket_index];

        if (
                HASHTABLE_KEY_VALUE_IS_EMPTY(key_value->flags) ||
                !HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags, HASHTABLE_KEY_VALUE_FLAG_FILLED)) {
            continue;
        }

        if (
                HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags, HASHTABLE_KEY_VALUE_FLAG_DELETED) ||
                HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags, HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE)) {
            continue;
        }

        fast_fixed_memory_allocator_mem_free(key_value->external_key.data);
    }
}

void hashtable_mcmp_data_free(
        hashtable_data_t* hashtable_data) {
    hashtable_mcmp_data_keys_free(hashtable_data);
    hashtable_mcmp_thread_counters_free(hashtable_data);

    xalloc_mmap_free((void*)hashtable_data->half_hashes_chunk, hashtable_data->half_hashes_chunk_size);
    xalloc_mmap_free((void*)hashtable_data->keys_values, hashtable_data->keys_values_size);
    xalloc_free((void*)hashtable_data);
}
