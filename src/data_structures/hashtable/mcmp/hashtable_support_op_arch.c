/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdatomic.h>
#include <assert.h>
#include <numa.h>

#include "exttypes.h"
#include "memory_fences.h"
#include "misc.h"
#include "log/log.h"
#include "spinlock.h"
#include "utils_string.h"

#include "hashtable.h"
#include "hashtable_support_index.h"
#include "hashtable_support_hash.h"
#include "hashtable_support_hash_search.h"

#ifndef CACHEGRAND_HASHTABLE_MCMP_SUPPORT_OP_ARCH_SUFFIX
#define CACHEGRAND_HASHTABLE_MCMP_SUPPORT_OP_ARCH_SUFFIX defaultopt
#endif

bool concat(hashtable_mcmp_support_op_search_key, CACHEGRAND_HASHTABLE_MCMP_SUPPORT_OP_ARCH_SUFFIX)(
        hashtable_data_volatile_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_hash_t hash,
        hashtable_chunk_index_t *found_chunk_index,
        hashtable_chunk_slot_index_t *found_chunk_slot_index,
        hashtable_key_value_volatile_t **found_key_value) {
    hashtable_hash_half_t hash_half;
    hashtable_slot_id_wrapper_t slot_id_wrapper = {0};
    hashtable_bucket_index_t bucket_index;
    hashtable_chunk_index_t chunk_index, chunk_index_start_initial;
    hashtable_chunk_slot_index_t chunk_slot_index;
    hashtable_half_hashes_chunk_volatile_t * half_hashes_chunk;
    hashtable_key_value_volatile_t* key_value;
    volatile hashtable_key_data_t* found_key;
    hashtable_key_size_t found_key_compare_size;
    uint32_t skip_indexes_mask;
    bool found = false;

    bucket_index = hashtable_mcmp_support_index_from_hash(hashtable_data->buckets_count, hash);
    chunk_index_start_initial = bucket_index / HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;
    hash_half = hashtable_mcmp_support_hash_half(hash);

    assert(bucket_index < hashtable_data->buckets_count);
    assert(chunk_index_start_initial < hashtable_data->chunks_count);

    MEMORY_FENCE_LOAD();

    half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index_start_initial];
    uint8_volatile_t overflowed_chunks_counter = half_hashes_chunk->metadata.overflowed_chunks_counter;

    LOG_DI("hash_half = %lu", hash_half);
    LOG_DI("bucket_index = %lu", bucket_index);
    LOG_DI("chunk_index_start = %lu", chunk_index_start_initial);
    LOG_DI("hashtable_data->chunks_count = %lu", hashtable_data->chunks_count);
    LOG_DI("hashtable_data->buckets_count = %lu", hashtable_data->buckets_count);
    LOG_DI("hashtable_data->buckets_count_real = %lu", hashtable_data->buckets_count_real);
    LOG_DI("half_hashes_chunk->write_lock.lock = %d", half_hashes_chunk->write_lock.lock);
    LOG_DI("half_hashes_chunk->metadata.is_full = %lu", half_hashes_chunk->metadata.is_full);
    LOG_DI("half_hashes_chunk->metadata.changes_counter = %lu", half_hashes_chunk->metadata.changes_counter);
    LOG_DI("half_hashes_chunk->metadata.overflowed_chunks_counter = %lu", overflowed_chunks_counter);

    slot_id_wrapper.filled = 1;
    slot_id_wrapper.quarter_hash = hashtable_mcmp_support_hash_quarter(hash_half);

    for(
            chunk_index = chunk_index_start_initial;
            chunk_index <= chunk_index_start_initial + overflowed_chunks_counter && found == false;
            chunk_index++) {
        assert(chunk_index < hashtable_data->chunks_count);

        slot_id_wrapper.distance = chunk_index - chunk_index_start_initial;

        LOG_DI("> slot_id_wrapper.slot_id = %lu", slot_id_wrapper.slot_id);
        LOG_DI("> slot_id_wrapper.filled = %lu", slot_id_wrapper.filled);
        LOG_DI("> slot_id_wrapper.distance = %lu", slot_id_wrapper.distance);
        LOG_DI("> slot_id_wrapper.quarter_hash = %lu", slot_id_wrapper.quarter_hash);
        LOG_DI("> chunk_index = %lu", chunk_index);

        half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index];

        skip_indexes_mask = 0;

        while(true) {
            // Ensure that the fresh-est half_hashes are going to be read
            MEMORY_FENCE_LOAD();

            chunk_slot_index = hashtable_mcmp_support_hash_search(
                    slot_id_wrapper.slot_id,
                    (hashtable_hash_half_volatile_t *) half_hashes_chunk->half_hashes,
                    skip_indexes_mask);

            LOG_DI(">> chunk_slot_index = %lu", chunk_slot_index);

            if (chunk_slot_index == HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_NOT_FOUND) {
                LOG_DI(">> no match found, exiting slot search loop");
                break;
            }

            // Update the skip indexes in case of another iteration
            skip_indexes_mask |= 1u << chunk_slot_index;

            LOG_DI(">> skip_indexes_mask = 0x%08x", skip_indexes_mask);
            LOG_DI(">> fetching key_value at bucket index = %lu",
                   (chunk_index * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT) + chunk_slot_index);

            key_value = &hashtable_data->keys_values[
                    (chunk_index * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT) + chunk_slot_index];

            // Ensure that the fresh-est flags value is going to be read in the next code block
            MEMORY_FENCE_LOAD();

            LOG_DI(">> key_value->flags = 0x%08x", key_value->flags);

            // Stop if hash found but bucket being filled, edge case because of parallelism.
            if (unlikely(HASHTABLE_KEY_VALUE_IS_EMPTY(key_value->flags))) {
                LOG_DI(">> key_value->flags = 0 - terminating loop");
                break;
            } else if (unlikely(HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags,
                    HASHTABLE_KEY_VALUE_FLAG_DELETED))) {
                LOG_DI(">> key_value->flags has HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED - continuing");
                continue;
            }

            LOG_DI(">> checking key");

            // The key may potentially change if the item is first deleted and then recreated, if it's inline it
            // doesn't really matter because the key will mismatch and the execution will continue but if the key is
            // stored externally and the allocated memory is freed it may crash.
            if (HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags,
                    HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE)) {
                LOG_DI(">> key_value->flags has HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE");

                found_key = key_value->inline_key.data;
                found_key_compare_size = key_value->inline_key.size;
            } else {
                LOG_DI(">> key_value->flags hasn't HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE");

                // TODO: The keys must be stored in an append only memory structure to avoid locking, memory can't
                //       be freed immediately after the bucket is freed because it can be in use and would cause a
                //       crash
                found_key = key_value->external_key.data;
                found_key_compare_size = key_value->external_key.size;

                if (key_value->external_key.size != key_size) {
                    LOG_DI(">> key have different length (%lu != %lu), skipping comparison",
                           key_size, key_value->external_key.size);
                    continue;
                }
            }

            // Ensure that the fresh-est flags value is going to be read to avoid that the deleted flag has
            // been set after they key pointer has been read
            MEMORY_FENCE_LOAD();

            // Check the flags after it fetches the key, if DELETED is set the flag that specifies the
            // inline of the key is not reliable anymore and therefore we may read from some memory not owned
            // by the software.
            if (unlikely(HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags,
                    HASHTABLE_KEY_VALUE_FLAG_DELETED))) {
                LOG_DI(">> key_value->flags has HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED - continuing");
                continue;
            }

            LOG_DI(">> key fetched, comparing");

            if (unlikely(utils_string_casecmp_eq_32(key, key_size, (const char*)found_key, found_key_compare_size) == false)) {
                LOG_DI(">> key different (%s != %s), unable to continue", key, found_key);
                continue;
            }

            *found_chunk_index = chunk_index;
            *found_chunk_slot_index = chunk_slot_index;
            *found_key_value = key_value;
            found = true;

            LOG_DI(">> match found, updating found_key_value");
            break;
        }
    }

    LOG_DI("found_chunk_index = %lu", *found_chunk_index);
    LOG_DI("found_chunk_slot_index = %lu", *found_chunk_slot_index);
    LOG_DI("found_key_value = 0x%016x", *found_key_value);
    LOG_DI("found = %s", found ? "YES" : "NO");

    return found;
}

bool concat(hashtable_mcmp_support_op_search_key_or_create_new, CACHEGRAND_HASHTABLE_MCMP_SUPPORT_OP_ARCH_SUFFIX)(
        hashtable_data_volatile_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_hash_t hash,
        bool create_new_if_missing,
        bool *created_new,
        hashtable_half_hashes_chunk_volatile_t **found_half_hashes_chunk,
        hashtable_key_value_volatile_t **found_key_value) {
    hashtable_hash_half_t hash_half;
    hashtable_slot_id_wrapper_t slot_id_wrapper = {0};
    hashtable_bucket_index_t bucket_index;
    hashtable_chunk_index_t chunk_index, chunk_index_start, chunk_index_start_initial, chunk_index_end,
        chunk_first_with_freespace, found_chunk_index = 0, locked_up_to_chunk_index = 0;
    hashtable_chunk_slot_index_t chunk_slot_index;
    hashtable_half_hashes_chunk_volatile_t* half_hashes_chunk;
    hashtable_key_value_volatile_t* key_value;
    volatile hashtable_key_data_t* found_key;
    hashtable_key_size_t found_key_compare_size;
    uint32_t skip_indexes_mask;
    bool found = false;
    bool found_chunk_with_freespace = false;

    bucket_index = hashtable_mcmp_support_index_from_hash(hashtable_data->buckets_count, hash);
    chunk_index_start = chunk_index_start_initial = bucket_index / HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;

    assert(bucket_index < hashtable_data->buckets_count);
    assert(chunk_index_start < hashtable_data->chunks_count);

    hash_half = hashtable_mcmp_support_hash_half(hash);

    LOG_DI("hash_half = %lu", hash_half);
    LOG_DI("bucket_index = %lu", bucket_index);
    LOG_DI("chunk_index_start = %lu", chunk_index_start);
    LOG_DI("hashtable_data->chunks_count = %lu", hashtable_data->chunks_count);
    LOG_DI("hashtable_data->buckets_count = %lu", hashtable_data->buckets_count);
    LOG_DI("hashtable_data->buckets_count_real = %lu", hashtable_data->buckets_count_real);

    half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index_start];

    spinlock_lock(&half_hashes_chunk->write_lock, true);
    locked_up_to_chunk_index = chunk_index_start;

    LOG_DI("locked_up_to_chunk_index = %lu", locked_up_to_chunk_index);

    uint8_volatile_t overflowed_chunks_counter = half_hashes_chunk->metadata.overflowed_chunks_counter;

    LOG_DI("half_hashes_chunk->write_lock.lock = %lu", half_hashes_chunk->write_lock.lock);
    LOG_DI("half_hashes_chunk->metadata.is_full = %lu", half_hashes_chunk->metadata.is_full);
    LOG_DI("half_hashes_chunk->metadata.changes_counter = %lu", half_hashes_chunk->metadata.changes_counter);
    LOG_DI("half_hashes_chunk->metadata.overflowed_chunks_counter = %lu", overflowed_chunks_counter);

    slot_id_wrapper.filled = 1;
    slot_id_wrapper.quarter_hash = hashtable_mcmp_support_hash_quarter(hash_half);

    for(
            uint8_t searching_or_creating = 0;
            (searching_or_creating < (create_new_if_missing ? 2 : 1)) && found == false;
            searching_or_creating++) {

        // Setup the search range
        if (searching_or_creating == 0) {
            // chunk_index_start has been calculated at the beginning of the function
            chunk_index_end = chunk_index_start + overflowed_chunks_counter + 1;
        } else {
            chunk_index_start = chunk_first_with_freespace;
            chunk_index_end =
                    chunk_index_start_initial +
                    HASHTABLE_HALF_HASHES_CHUNK_SEARCH_MAX;
        }

        LOG_DI("> searching_or_creating = %s", searching_or_creating == 0 ? "SEARCHING" : "CREATING");
        LOG_DI("> chunk_index_start = %u", chunk_index_start);
        LOG_DI("> chunk_index_end = %u", chunk_index_end);

        assert(chunk_index_start <= hashtable_data->chunks_count);
        assert(chunk_index_end <= hashtable_data->chunks_count);

        for (
                chunk_index = chunk_index_start;
                chunk_index < chunk_index_end && found == false;
                chunk_index++) {
            assert(chunk_index < hashtable_data->chunks_count);

            // Update the distance in the slot id wrapper
            slot_id_wrapper.distance = chunk_index - chunk_index_start_initial;

            LOG_DI("> slot_id_wrapper.slot_id = %lu", slot_id_wrapper.slot_id);
            LOG_DI("> slot_id_wrapper.filled = %lu", slot_id_wrapper.filled);
            LOG_DI("> slot_id_wrapper.distance = %lu", slot_id_wrapper.distance);
            LOG_DI("> slot_id_wrapper.quarter_hash = %lu", slot_id_wrapper.quarter_hash);
            LOG_DI(">> chunk_index = %lu", chunk_index);

            half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index];

            // Every time a new chunk gets processed during the search it has to be locked for safety reasons
            LOG_DI(">> chunk_index > locked_up_to_chunk_index = %s", chunk_index > locked_up_to_chunk_index ? "YES" : "NO");
            if (chunk_index > locked_up_to_chunk_index) {
                LOG_DI(">> updating locked_up_to_chunk_index to %d", chunk_index);
                locked_up_to_chunk_index = chunk_index;

                LOG_DI(">> locking chunk (with retry)");
                spinlock_lock(&half_hashes_chunk->write_lock, true);
            }

            if (searching_or_creating == 0) {
                // Check if it has found a chunk with free space
                if (found_chunk_with_freespace == false) {
                    chunk_first_with_freespace = chunk_index;
                    LOG_DI(">> chunk_first_with_freespace = %lu", chunk_first_with_freespace);

                    if (half_hashes_chunk->metadata.is_full == 0) {
                        LOG_DI(">> found chunk with free space");
                        found_chunk_with_freespace = true;
                    }
                }
            }

            skip_indexes_mask = 0;

            while (true) {
                // It's not necessary to have a memory fence here, these data are not going to change because of the
                // write lock and a full barrier is issued by the lock operation
                chunk_slot_index = hashtable_mcmp_support_hash_search(
                        searching_or_creating == 0 ? slot_id_wrapper.slot_id : 0,
                        (hashtable_hash_half_volatile_t *) half_hashes_chunk->half_hashes,
                        skip_indexes_mask);

                LOG_DI(">>> chunk_slot_index = %lu", chunk_slot_index);

                if (chunk_slot_index == HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_NOT_FOUND) {
                    break;
                }

                // Update the skip indexes in case of another iteration
                skip_indexes_mask |= 1u << chunk_slot_index;

                LOG_DI(">>> skip_indexes_mask = 0x%08x", skip_indexes_mask);
                LOG_DI(">>> fetching key_value at bucket index = %lu",
                       (chunk_index * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT) + chunk_slot_index);

                key_value = &hashtable_data->keys_values[
                        (chunk_index * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT) + chunk_slot_index];

                LOG_DI(">>> key_value->flags = 0x%08x", key_value->flags);

                if (searching_or_creating == 0) {
                    if (HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags,
                            HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE)) {
                        LOG_DI(">>> key_value->flags has HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE");

                        found_key = key_value->inline_key.data;
                        found_key_compare_size = key_value->inline_key.size;
                    } else {
                        LOG_DI(">>> key_value->flags hasn't HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE");

                        found_key = key_value->external_key.data;
                        found_key_compare_size = key_value->external_key.size;

                        if (unlikely(key_value->external_key.size != key_size)) {
                            LOG_DI(">>> key have different length (%lu != %lu), skipping comparison",
                                     key_size, key_value->external_key.size);
                            continue;
                        }
                    }

                    LOG_DI(">>> key fetched, comparing");

                    if (unlikely(utils_string_casecmp_eq_32(key, key_size, (const char*)found_key, found_key_compare_size) == false)) {
                        LOG_DI(">>> key different (%s != %s), unable to continue", key, found_key);
                        continue;
                    }

                    LOG_DI(">>> match found");
                } else {
                    // Not needed to perform a memory store barrier here, at some point the cpu will flush or a memory
                    // barrier will be issued. Setting the value without flushing is not impacting the functionalities.
                    half_hashes_chunk->half_hashes[chunk_slot_index].slot_id = slot_id_wrapper.slot_id;
                    *created_new = true;

                    LOG_DI(">>> empty slot found, updating the half_hashes in the chunk with the hash");
                }

                // Update the changes counter to the current chunk
                half_hashes_chunk->metadata.changes_counter++;
                LOG_DI(">>> incrementing the changes counter to %d", half_hashes_chunk->metadata.changes_counter);

                *found_half_hashes_chunk = half_hashes_chunk;
                *found_key_value = key_value;
                found_chunk_index = chunk_index;
                found = true;

                LOG_DI(">>> updating found_chunk_index and found_key_value");

                // Update the overflowed_chunks_counter if necessary
                uint8_volatile_t overflowed_chunks_counter_new = (uint8_t)(found_chunk_index - chunk_index_start_initial);
                uint8_volatile_t overflowed_chunks_counter_current =
                        hashtable_data->half_hashes_chunk[chunk_index_start_initial].metadata.overflowed_chunks_counter;
                uint8_volatile_t overflowed_chunks_counter_update = max(
                        overflowed_chunks_counter_new, overflowed_chunks_counter_current);

                hashtable_data->half_hashes_chunk[chunk_index_start_initial].metadata.overflowed_chunks_counter =
                        overflowed_chunks_counter_update;

                LOG_DI(">>> updating overflowed_chunks_counter to %lu", overflowed_chunks_counter_update);

                assert(overflowed_chunks_counter_update < HASHTABLE_HALF_HASHES_CHUNK_SEARCH_MAX);
                break;
            }

            if (searching_or_creating == 1) {
                if (found == false) {
                    LOG_DI(">> can't find a free slot in current chunk, setting is_full to 1");

                    // Not needed to perform a memory store barrier here, the is_full metadata is used only by the
                    // the thread holding the lock
                    half_hashes_chunk->metadata.is_full = 1;
                }
            }
        }
    }

    // Iterate of the chunks to remove the place locks, the only lock not removed is if the chunk holding the hash

    if (found) {
        LOG_DI("chunk %lu will not be unlocked, it has to be returned to the caller", found_chunk_index);
    }

    // TODO: refactor the code to have a sliding locking window, if the algorithm is not finding free slots there are
    //       no reasons to keep a chunk locked
    LOG_DI("unlocking chunks from %lu to %lu", chunk_index_start_initial, locked_up_to_chunk_index);
    for (chunk_index = chunk_index_start_initial; chunk_index <= locked_up_to_chunk_index; chunk_index++) {
        LOG_DI("> processing chunk %lu", chunk_index);
        if (found == true && chunk_index == found_chunk_index) {
            LOG_DI("> chunk to return to the caller, keeping it locked");
            continue;
        }
        half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index];

        LOG_DI("> unlocking chunk");
        spinlock_unlock(&half_hashes_chunk->write_lock);
    }

    LOG_DI("found_half_hashes_chunk = 0x%016x", *found_half_hashes_chunk);
    LOG_DI("found_key_value = 0x%016x", *found_key_value);
    LOG_DI("created_new = %s", *created_new ? "YES" : "NO");
    LOG_DI("found = %s", found ? "YES" : "NO");

    return found;
}
