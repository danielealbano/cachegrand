/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdatomic.h>
#include <assert.h>
#include <time.h>

#include "exttypes.h"
#include "memory_fences.h"
#include "misc.h"
#include "log/log.h"
#include "spinlock.h"
#include "transaction.h"

#include "hashtable.h"
#include "hashtable_support_index.h"
#include "hashtable_support_hash.h"

#ifndef CACHEGRAND_HASHTABLE_MCMP_SUPPORT_OP_ARCH_SUFFIX
#define CACHEGRAND_HASHTABLE_MCMP_SUPPORT_OP_ARCH_SUFFIX loop
#endif

#include WRAPFORINCLUDE(CONCAT(hashtable_support_hash_search,CACHEGRAND_HASHTABLE_MCMP_SUPPORT_OP_ARCH_SUFFIX).h)
#define HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_FUNC CONCAT(CONCAT(hashtable_mcmp_support_hash_search, CACHEGRAND_HASHTABLE_MCMP_SUPPORT_OP_ARCH_SUFFIX), 14)

bool CONCAT(hashtable_mcmp_support_op_search_key, CACHEGRAND_HASHTABLE_MCMP_SUPPORT_OP_ARCH_SUFFIX)(
        hashtable_data_volatile_t *hashtable_data,
        hashtable_database_number_t database_number,
        hashtable_key_data_t *key,
        hashtable_key_length_t key_length,
        hashtable_hash_t hash,
        transaction_t *transaction,
        hashtable_chunk_index_t *found_chunk_index,
        hashtable_chunk_slot_index_t *found_chunk_slot_index,
        hashtable_key_value_volatile_t **found_key_value) {
    hashtable_hash_half_t hash_half;
    hashtable_slot_id_wrapper_t slot_id_wrapper = { 0 };
    hashtable_bucket_index_t bucket_index;
    hashtable_chunk_index_t chunk_index, chunk_index_start_initial;
    hashtable_chunk_slot_index_t chunk_slot_index;
    hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk;
    hashtable_key_value_volatile_t *key_value;
    hashtable_key_data_volatile_t *found_key;
    hashtable_key_length_t found_key_length;
    uint32_t skip_indexes_mask;
    uint8_volatile_t overflowed_chunks_counter;
    bool found = false;

    bucket_index = hashtable_mcmp_support_index_from_hash(hashtable_data->buckets_count, hash);
    chunk_index_start_initial = bucket_index / HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;
    hash_half = hashtable_mcmp_support_hash_half(hash);

    assert(bucket_index < hashtable_data->buckets_count);
    assert(chunk_index_start_initial < hashtable_data->chunks_count);

    half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index_start_initial];

    // Lock the chunk for reading to get a consistent view of the data and ensure that overflowed_chunks_counter is
    // up to date
    if (unlikely(!transaction_rwspinlock_is_owned_by_transaction(&half_hashes_chunk->lock, transaction))) {
        if (unlikely(!transaction_lock_for_read(transaction, &half_hashes_chunk->lock))) {
            return false;
        }
    }

    overflowed_chunks_counter = half_hashes_chunk->metadata.overflowed_chunks_counter;

    slot_id_wrapper.filled = 1;
    slot_id_wrapper.quarter_hash = hashtable_mcmp_support_hash_quarter(hash_half);

    for(
            chunk_index = chunk_index_start_initial;
            chunk_index <= chunk_index_start_initial + overflowed_chunks_counter && found == false;
            chunk_index++) {
        assert(chunk_index < hashtable_data->chunks_count);

        slot_id_wrapper.distance = chunk_index - chunk_index_start_initial;

        half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index];
        if (half_hashes_chunk->metadata.slots_occupied == 0) {
            continue;
        }

        // The first chunk is locked initially to read the overflowed_chunks_counter
        if (likely(chunk_index != chunk_index_start_initial)) {
            // Increment the readers counter (it will issue an atomic increment which will sync the cachelines as needed
            // mimicking a memory fence)
            if (unlikely(!transaction_rwspinlock_is_owned_by_transaction(&half_hashes_chunk->lock, transaction))) {
                if (unlikely(!transaction_lock_for_read(transaction, &half_hashes_chunk->lock))) {
                    return false;
                }
            }
        }

        skip_indexes_mask = 0;
        uint32_t result_mask = HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_FUNC(
                slot_id_wrapper.slot_id,
                (hashtable_hash_half_volatile_t *) half_hashes_chunk->half_hashes);
        while(true) {
            uint32_t skip_indexes_mask_inv = ~(skip_indexes_mask | 0xC000);
            if ((result_mask & skip_indexes_mask_inv) == 0) {
                break;
            }
            chunk_slot_index = __builtin_ctz(result_mask & skip_indexes_mask_inv);

            // Update the skip indexes in case of another iteration
            skip_indexes_mask |= 1u << chunk_slot_index;

            key_value = &hashtable_data->keys_values[
                    (chunk_index * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT) + chunk_slot_index];

            // Stop if hash found but bucket being filled, edge case because of parallelism.
            if (unlikely(HASHTABLE_KEY_VALUE_IS_EMPTY(key_value->flags))) {
                break;
            } else if (unlikely(HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags, HASHTABLE_KEY_VALUE_FLAG_DELETED))) {
                continue;
            }

            found_key = key_value->key;
            found_key_length = key_value->key_length;

            if (found_key_length != key_length) {
                continue;
            }

            // Check the flags after it fetches the key, if DELETED is set the flag that indicates that the key is
            // inlined is not reliable anymore, therefore we may read from some memory not owned by the software.
            if (unlikely(HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags,
                    HASHTABLE_KEY_VALUE_FLAG_DELETED))) {
                continue;
            }

            if (
                    key_length != found_key_length
                    ||
                    strncmp(key, (const char*)found_key, found_key_length) != 0
                    ||
                    database_number != key_value->database_number) {
                continue;
            }

            *found_chunk_index = chunk_index;
            *found_chunk_slot_index = chunk_slot_index;
            *found_key_value = key_value;
            found = true;

            break;
        }
    }

    return found;
}

bool CONCAT(hashtable_mcmp_support_op_search_key_or_create_new, CACHEGRAND_HASHTABLE_MCMP_SUPPORT_OP_ARCH_SUFFIX)(
        hashtable_data_volatile_t *hashtable_data,
        hashtable_database_number_t database_number,
        hashtable_key_data_t *key,
        hashtable_key_length_t key_length,
        hashtable_hash_t hash,
        bool create_new_if_missing,
        transaction_t *transaction,
        bool *created_new,
        hashtable_chunk_index_t *found_chunk_index,
        hashtable_half_hashes_chunk_volatile_t **found_half_hashes_chunk,
        hashtable_chunk_slot_index_t *found_chunk_slot_index,
        hashtable_key_value_volatile_t **found_key_value) {
    hashtable_hash_half_t hash_half;
    hashtable_slot_id_wrapper_t slot_id_wrapper = {0};
    hashtable_bucket_index_t bucket_index;
    hashtable_chunk_index_t chunk_index, chunk_index_start, chunk_index_start_initial, chunk_index_end,
        chunk_first_with_freespace, locked_up_to_chunk_index = 0;
    hashtable_chunk_slot_index_t chunk_slot_index;
    hashtable_half_hashes_chunk_volatile_t* half_hashes_chunk;
    hashtable_key_value_volatile_t* key_value;
    hashtable_key_data_volatile_t* found_key;
    hashtable_key_length_volatile_t found_key_length;

    uint32_t skip_indexes_mask;
    bool found = false;
    bool found_chunk_with_freespace = false;
    *created_new = false;
    *found_chunk_index = 0;
    *found_chunk_slot_index = 0;

    assert(transaction->transaction_id.id != TRANSACTION_ID_NOT_ACQUIRED);

    bucket_index = hashtable_mcmp_support_index_from_hash(hashtable_data->buckets_count, hash);
    chunk_index_start = chunk_index_start_initial = bucket_index / HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;

    assert(bucket_index < hashtable_data->buckets_count);
    assert(chunk_index_start < hashtable_data->chunks_count);

    hash_half = hashtable_mcmp_support_hash_half(hash);
    half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index_start];

    if (likely(!transaction_rwspinlock_is_owned_by_transaction(&half_hashes_chunk->lock, transaction))) {
        if (unlikely(!transaction_lock_for_write(transaction, &half_hashes_chunk->lock))) {
            return false;
        }
    }

    locked_up_to_chunk_index = chunk_index_start;
    uint8_volatile_t overflowed_chunks_counter = half_hashes_chunk->metadata.overflowed_chunks_counter;

    slot_id_wrapper.filled = 1;
    slot_id_wrapper.quarter_hash = hashtable_mcmp_support_hash_quarter(hash_half);

    for(
            uint8_t searching_or_creating = 0;
            (searching_or_creating < (create_new_if_missing ? 2 : 1)) && found == false;
            searching_or_creating++) {

        // Set up the search range
        if (searching_or_creating == 0) {
            // chunk_index_start has been calculated at the beginning of the function
            chunk_index_end = chunk_index_start + overflowed_chunks_counter + 1;
        } else {
            chunk_index_start = chunk_first_with_freespace;
            chunk_index_end =
                    chunk_index_start_initial +
                    HASHTABLE_HALF_HASHES_CHUNK_SEARCH_MAX;
        }

        assert(chunk_index_start <= hashtable_data->chunks_count);
        assert(chunk_index_end <= hashtable_data->chunks_count);

        for (
                chunk_index = chunk_index_start;
                chunk_index < chunk_index_end && found == false;
                chunk_index++) {
            assert(chunk_index < hashtable_data->chunks_count);

            // Update the distance in the slot id wrapper
            slot_id_wrapper.distance = chunk_index - chunk_index_start_initial;

            half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index];

            // Every time a new chunk gets processed during the search it has to be locked for safety reasons
            if (chunk_index > locked_up_to_chunk_index) {
                locked_up_to_chunk_index = chunk_index;

                if (likely(!transaction_rwspinlock_is_owned_by_transaction(&half_hashes_chunk->lock, transaction))) {
                    if (unlikely(!transaction_lock_for_write(transaction, &half_hashes_chunk->lock))) {
                        return false;
                    }
                }
            }

            if (searching_or_creating == 0) {
                // Check if it has found a chunk with free space
                if (found_chunk_with_freespace == false) {
                    chunk_first_with_freespace = chunk_index;
                    if (half_hashes_chunk->metadata.is_full == 0) {
                        found_chunk_with_freespace = true;
                    }
                }

                if (half_hashes_chunk->metadata.slots_occupied == 0) {
                    continue;
                }
            }

            skip_indexes_mask = 0;
            uint32_t result_mask = HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_FUNC(
                    searching_or_creating == 0 ? slot_id_wrapper.slot_id : 0,
                    (hashtable_hash_half_volatile_t *) half_hashes_chunk->half_hashes);

            while (true) {
                uint32_t skip_indexes_mask_inv = ~(skip_indexes_mask | 0xC000);
                if ((result_mask & skip_indexes_mask_inv) == 0) {
                    break;
                }

                chunk_slot_index = __builtin_ctz(result_mask & skip_indexes_mask_inv);

                // Update the skip indexes in case of another iteration
                skip_indexes_mask |= 1u << chunk_slot_index;

                key_value = &hashtable_data->keys_values[
                        (chunk_index * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT) + chunk_slot_index];

                if (searching_or_creating == 0) {
                    found_key = key_value->key;
                    found_key_length = key_value->key_length;

                    if (unlikely(found_key_length != key_length)) {
                        continue;
                    }

                    // Issue a memory fence and compare the database number to ensure that the entry hasn't been changed
                    // and assigned to another database
                    if (
                            key_length != found_key_length
                            ||
                            strncmp(key, (const char*)found_key, found_key_length) != 0
                            ||
                            database_number != key_value->database_number) {
                        continue;
                    }
                } else {
                    // Not needed to perform a memory store barrier here, at some point the cpu will flush or a memory
                    // barrier will be issued. Setting the value without flushing is not impacting the functionalities.
                    half_hashes_chunk->half_hashes[chunk_slot_index].slot_id = slot_id_wrapper.slot_id;
                    *created_new = true;

                    // Update the counter for the occupied slots
                    half_hashes_chunk->metadata.slots_occupied++;
                    assert(half_hashes_chunk->metadata.slots_occupied <= HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);
                }

                *found_half_hashes_chunk = half_hashes_chunk;
                *found_key_value = key_value;
                *found_chunk_index = chunk_index;
                *found_chunk_slot_index = chunk_slot_index;
                found = true;

                // Update the overflowed_chunks_counter if necessary
                uint8_volatile_t overflowed_chunks_counter_new = (uint8_t)(*found_chunk_index - chunk_index_start_initial);
                uint8_volatile_t overflowed_chunks_counter_current =
                        hashtable_data->half_hashes_chunk[chunk_index_start_initial].metadata.overflowed_chunks_counter;
                uint8_volatile_t overflowed_chunks_counter_update = MAX(
                        overflowed_chunks_counter_new, overflowed_chunks_counter_current);

                hashtable_data->half_hashes_chunk[chunk_index_start_initial].metadata.overflowed_chunks_counter =
                        overflowed_chunks_counter_update;

                assert(overflowed_chunks_counter_update < HASHTABLE_HALF_HASHES_CHUNK_SEARCH_MAX);
                break;
            }

            if (searching_or_creating == 1) {
                if (found == false) {
                    // Not needed to perform a memory store barrier here, the is_full metadata is used only by the
                    // thread holding the lock
                    half_hashes_chunk->metadata.is_full = 1;
                }
            }
        }
    }

    return found;
}
