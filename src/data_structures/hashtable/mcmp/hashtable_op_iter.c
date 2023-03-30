/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "hashtable.h"
#include "hashtable_data.h"

#include "hashtable_op_iter.h"

void *hashtable_mcmp_op_data_iter(
        hashtable_data_volatile_t *hashtable_data,
        uint64_t *bucket_index) {
    hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk;
    hashtable_key_value_volatile_t *key_value;
    hashtable_chunk_index_t chunk_index, chunk_index_start, chunk_index_end;
    hashtable_chunk_slot_index_t chunk_slot_index;

    chunk_index_start = *bucket_index / HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;
    chunk_index_end = hashtable_data->chunks_count;
    chunk_slot_index = *bucket_index % HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;

    for(
            chunk_index = chunk_index_start;
            chunk_index < chunk_index_end;
            chunk_index++) {
        half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index];

        for(; chunk_slot_index < HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT; chunk_slot_index++) {
            MEMORY_FENCE_LOAD();
            *bucket_index = (chunk_index * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT) + chunk_slot_index;

            assert(*bucket_index < hashtable_data->buckets_count_real);

            // If there is no slot_id (hash plus other metadata) the bucket is empty and can be skipped
            if (half_hashes_chunk->half_hashes[chunk_slot_index].slot_id == 0) {
                continue;
            }

            key_value = &hashtable_data->keys_values[*bucket_index];
            volatile void* data = (void*)key_value->data;

            MEMORY_FENCE_LOAD();
            if (
                    HASHTABLE_KEY_VALUE_IS_EMPTY(key_value->flags) ||
                    HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags, HASHTABLE_KEY_VALUE_FLAG_DELETED)) {
                continue;
            }

            return (void*)data;
        }

        chunk_slot_index = 0;
    }

    return NULL;
}

void *hashtable_mcmp_op_iter(
        hashtable_t *hashtable,
        uint64_t *bucket_index) {
    return hashtable_mcmp_op_data_iter(hashtable->ht_current, bucket_index);
}
