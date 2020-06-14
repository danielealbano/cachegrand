#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <sched.h>
#include <assert.h>

#include "misc.h"
#include "log.h"
#include "memory_fences.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_index.h"
#include "hashtable/hashtable_support_hash.h"
#include "hashtable/hashtable_support_op.h"
#include "hashtable/hashtable_support_op_arch.h"
#include "hashtable/hashtable_support_hash_search.h"

#ifndef CACHEGRAND_HASHTABLE_SUPPORT_OP_ARCH_SUFFIX
#error "CACHEGRAND_HASHTABLE_SUPPORT_OP_ARCH_SUFFIX not defined, unable to build"
#endif

bool concat(hashtable_support_op_search_key, CACHEGRAND_HASHTABLE_SUPPORT_OP_ARCH_SUFFIX)(
        volatile hashtable_data_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_hash_t hash,
        volatile hashtable_key_value_t **found_key_value) {
    hashtable_hash_half_t hash_half;
    hashtable_bucket_index_t bucket_index;
    hashtable_chunk_index_t chunk_index, chunk_index_start;
    hashtable_chunk_slot_index_t chunk_slot_index;
    hashtable_half_hashes_chunk_atomic_t * half_hashes_chunk;
    volatile hashtable_key_value_t* key_value;
    volatile hashtable_key_data_t* found_key;
    hashtable_key_size_t found_key_max_compare_size;
    uint32_t skip_indexes_mask;
    bool found = false;

    bucket_index = hashtable_support_index_from_hash(hashtable_data->buckets_count, hash);
    chunk_index_start = bucket_index / HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT;
    hash_half = hashtable_support_hash_half(hash);

    assert(bucket_index < hashtable_data->buckets_count);
    assert(chunk_index_start < hashtable_data->chunks_count);

    HASHTABLE_MEMORY_FENCE_LOAD();

    half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index_start];
    uint8_atomic_t overflowed_chunks_counter = half_hashes_chunk->metadata.overflowed_chunks_counter;

    LOG_DI("hash_half = %lu", hash_half);
    LOG_DI("bucket_index = %lu", bucket_index);
    LOG_DI("chunk_index_start = %lu", chunk_index_start);
    LOG_DI("hashtable_data->chunks_count = %lu", hashtable_data->chunks_count);
    LOG_DI("hashtable_data->buckets_count = %lu", hashtable_data->buckets_count);
    LOG_DI("hashtable_data->buckets_count_real = %lu", hashtable_data->buckets_count_real);
    LOG_DI("half_hashes_chunk->metadata.write_lock = %lu", half_hashes_chunk->metadata.write_lock);
    LOG_DI("half_hashes_chunk->metadata.is_full = %lu", half_hashes_chunk->metadata.is_full);
    LOG_DI("half_hashes_chunk->metadata.changes_counter = %lu", half_hashes_chunk->metadata.changes_counter);
    LOG_DI("half_hashes_chunk->metadata.overflowed_chunks_counter = %lu", overflowed_chunks_counter);

    for(
            chunk_index = chunk_index_start;
            chunk_index <= chunk_index_start + overflowed_chunks_counter && found == false;
            chunk_index++) {
        assert(chunk_index < hashtable_data->chunks_count);

        LOG_DI("> chunk_index = %lu", chunk_index);

        half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index];

        skip_indexes_mask = 0;

        while(true) {
            HASHTABLE_MEMORY_FENCE_LOAD();

            chunk_slot_index = hashtable_support_hash_search(
                hash_half,
                (hashtable_hash_half_atomic_t*)half_hashes_chunk->half_hashes,
                skip_indexes_mask);

            LOG_DI(">> chunk_slot_index = %lu", chunk_slot_index);

            if (chunk_slot_index == HASHTABLE_SUPPORT_HASH_SEARCH_NOT_FOUND) {
                LOG_DI(">> no match found, exiting slot search loop");
                break;
            }

            // Update the skip indexes in case of another iteration
            skip_indexes_mask |= 1u << chunk_slot_index;

            LOG_DI(">> skip_indexes_mask = 0x%08x", skip_indexes_mask);
            LOG_DI(">> fetching key_value at bucket index = %lu",
                    (chunk_index * HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT) + chunk_slot_index);

            key_value = &hashtable_data->keys_values[
                    (chunk_index * HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT) + chunk_slot_index];

            LOG_DI(">> key_value->flags = 0x%08x", key_value->flags);

            // Stop if hash found but bucket being filled, edge case because of parallelism.
            if (HASHTABLE_KEY_VALUE_IS_EMPTY(key_value->flags)) {
                LOG_DI(">> key_value->flags = 0 - terminating loop");
                break;
            } else if (HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags,
                                                    HASHTABLE_KEY_VALUE_FLAG_DELETED)) {
                LOG_DI(">> key_value->flags has HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED - continuing");
                continue;
            } else {
                LOG_DI(">> checking key");
                // The key may potentially change if the item is first deleted and then recreated, if it's inline it
                // doesn't really matter because the key will mismatch and the execution will continue but if the key is
                // stored externally and the allocated memory is freed it may crash.
                if (HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags,
                                                 HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE)) {
                    LOG_DI(">> key_value->flags has HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE");

                    found_key = key_value->inline_key.data;
                    found_key_max_compare_size = HASHTABLE_KEY_INLINE_MAX_LENGTH;
                } else {
                    LOG_DI(">> key_value->flags hasn't HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE");

    #if defined(CACHEGRAND_HASHTABLE_KEY_CHECK_FULL)
                    LOG_DI(">> CACHEGRAND_HASHTABLE_KEY_CHECK_FULL defined, comparing full key");

                    // TODO: The keys must be stored in an append only memory structure to avoid locking, memory can't
                    //       be freed immediately after the bucket is freed because it can be in use and would cause a
                    //       crash
                    found_key = key_value->external_key.data;
                    found_key_max_compare_size = key_value->external_key.size;

                    if (key_value->external_key.size != key_size) {
                        LOG_DI(">> key have different length (%lu != %lu), continuing",
                                 key_size, key_value->external_key.size);
                        continue;
                    }
    #else
                    LOG_DI(">> CACHEGRAND_HASHTABLE_KEY_CHECK_FULL not defined, comparing only key prefix");

                    found_key = key_value->prefix_key.data;
                    found_key_max_compare_size = HASHTABLE_KEY_PREFIX_SIZE;

                    if (key_value->prefix_key.size != key_size) {
                        LOG_DI(">> key have different length (%lu != %lu), continuing",
                                key_size, key_value->prefix_key.size);
                        continue;
                    }
    #endif // CACHEGRAND_HASHTABLE_KEY_CHECK_FULL
                }

                LOG_DI(">> key fetched, comparing");

                if (strncmp(key, (const char *) found_key, found_key_max_compare_size) != 0) {
                    char* temp_found_key = (char*)malloc(found_key_max_compare_size + 1);
                    temp_found_key[found_key_max_compare_size] = 0;
                    strncpy(temp_found_key, (char*)found_key, found_key_max_compare_size);

                    LOG_DI(">> key different (%s != %s), unable to continue", key, found_key);
                    continue;
                }
            }

            *found_key_value = key_value;
            found = true;

            LOG_DI(">> match found, updating found_key_value");
            break;
        }
    }

    LOG_DI("found_key_value = 0x%016x", *found_key_value);
    LOG_DI("found = %s", found ? "YES" : "NO");

    return found;
}

bool concat(hashtable_support_op_half_hashes_chunk_lock, CACHEGRAND_HASHTABLE_SUPPORT_OP_ARCH_SUFFIX)(
        hashtable_half_hashes_chunk_atomic_t * half_hashes_chunk,
        bool retry) {
    bool write_lock_set = false;

    LOG_DI("trying to lock half_hashes_chunk 0x%016x", half_hashes_chunk);
    LOG_DI("retry  = %s", retry ? "YES" : "NO");

    do {
        uint8_t expected_value = 0;

        write_lock_set = __sync_bool_compare_and_swap(
                &half_hashes_chunk->metadata.write_lock,
                expected_value,
                1);

        if (!write_lock_set) {
            LOG_DI("failed to lock, waiting");
            sched_yield();
        }
    } while(retry && !write_lock_set);

    return write_lock_set;
}

void concat(hashtable_support_op_half_hashes_chunk_unlock, CACHEGRAND_HASHTABLE_SUPPORT_OP_ARCH_SUFFIX)(
        hashtable_half_hashes_chunk_atomic_t* half_hashes_chunk) {
    LOG_DI("unlocking half_hashes_chunk 0x%016x", half_hashes_chunk);

    half_hashes_chunk->metadata.write_lock = 0;
    HASHTABLE_MEMORY_FENCE_STORE();
}

bool concat(hashtable_support_op_search_key_or_create_new, CACHEGRAND_HASHTABLE_SUPPORT_OP_ARCH_SUFFIX)(
        hashtable_data_atomic_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_hash_t hash,
        bool create_new_if_missing,
        bool *created_new,
        hashtable_half_hashes_chunk_atomic_t **found_half_hashes_chunk,
        volatile hashtable_key_value_t **found_key_value) {
    hashtable_hash_half_t hash_half;
    hashtable_hash_half_t search_hash_half;
    hashtable_bucket_index_t bucket_index;
    hashtable_chunk_index_t chunk_index, chunk_index_start, chunk_index_start_initial, chunk_index_end,
        chunk_first_with_freespace, found_chunk_index = 0, locked_up_to_chunk_index = 0;
    hashtable_chunk_slot_index_t chunk_slot_index;
    hashtable_half_hashes_chunk_atomic_t* half_hashes_chunk;
    volatile hashtable_key_value_t* key_value;
    volatile hashtable_key_data_t* found_key;
    hashtable_key_size_t found_key_max_compare_size;
    uint32_t skip_indexes_mask;
    bool found = false;
    bool found_chunk_with_freespace = false;

    bucket_index = hashtable_support_index_from_hash(hashtable_data->buckets_count, hash);
    chunk_index_start = chunk_index_start_initial = bucket_index / HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT;

    assert(bucket_index < hashtable_data->buckets_count);
    assert(chunk_index_start < hashtable_data->chunks_count);

    hash_half = hashtable_support_hash_half(hash);

    LOG_DI("hash_half = %lu", hash_half);
    LOG_DI("bucket_index = %lu", bucket_index);
    LOG_DI("chunk_index_start = %lu", chunk_index_start);
    LOG_DI("hashtable_data->chunks_count = %lu", hashtable_data->chunks_count);
    LOG_DI("hashtable_data->buckets_count = %lu", hashtable_data->buckets_count);
    LOG_DI("hashtable_data->buckets_count_real = %lu", hashtable_data->buckets_count_real);

    half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index_start];

    concat(hashtable_support_op_half_hashes_chunk_lock, CACHEGRAND_HASHTABLE_SUPPORT_OP_ARCH_SUFFIX)(half_hashes_chunk, true);
    locked_up_to_chunk_index = chunk_index_start;

    LOG_DI("locked_up_to_chunk_index = %lu", locked_up_to_chunk_index);

    uint8_atomic_t overflowed_chunks_counter = half_hashes_chunk->metadata.overflowed_chunks_counter;

    LOG_DI("half_hashes_chunk->metadata.write_lock = %lu", half_hashes_chunk->metadata.write_lock);
    LOG_DI("half_hashes_chunk->metadata.is_full = %lu", half_hashes_chunk->metadata.is_full);
    LOG_DI("half_hashes_chunk->metadata.changes_counter = %lu", half_hashes_chunk->metadata.changes_counter);
    LOG_DI("half_hashes_chunk->metadata.overflowed_chunks_counter = %lu", overflowed_chunks_counter);

    for(
            uint8_t searching_or_creating = 0;
            (searching_or_creating < (create_new_if_missing ? 2 : 1)) && found == false;
            searching_or_creating++) {

        // Setup the search range
        if (searching_or_creating == 0) {
            // chunk_index_start has been calculated at the beginning of the function
            chunk_index_end = chunk_index_start + overflowed_chunks_counter;
        } else {
            chunk_index_start = chunk_first_with_freespace;
            chunk_index_end =
                    chunk_index_start_initial +
                    HASHTABLE_HALF_HASHES_CHUNK_SEARCH_MAX;
        }

        LOG_DI("> searching_or_creating = %s", searching_or_creating == 0 ? "SEARCHING" : "CREATING");
        LOG_DI("> chunk_index_start = %u", chunk_index_start);
        LOG_DI("> chunk_index_end = %u", chunk_index_end);

        assert(chunk_index_start < hashtable_data->chunks_count);
        assert(chunk_index_end < hashtable_data->chunks_count);

        // Setup the search half hash
        if (searching_or_creating == 0) {
            search_hash_half = hash_half;
        } else {
            search_hash_half = 0;
        }

        LOG_DI("> search_hash_half = %u", search_hash_half);

        for (
                chunk_index = chunk_index_start;
                chunk_index <= chunk_index_end && found == false;
                chunk_index++) {
            assert(chunk_index < hashtable_data->chunks_count);

            LOG_DI(">> chunk_index = %lu", chunk_index);

            half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index];

            // Every time a new chunk gets processed during the search it has to be locked for safety reasons
            LOG_DI(">> chunk_index > locked_up_to_chunk_index = %s", chunk_index > locked_up_to_chunk_index ? "YES" : "NO");
            if (chunk_index > locked_up_to_chunk_index) {
                LOG_DI(">> updating locked_up_to_chunk_index to %d", chunk_index);
                locked_up_to_chunk_index = chunk_index;

                LOG_DI(">> locking chunk (with retry)");
                concat(hashtable_support_op_half_hashes_chunk_lock, CACHEGRAND_HASHTABLE_SUPPORT_OP_ARCH_SUFFIX)(half_hashes_chunk, true);
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
                chunk_slot_index = hashtable_support_hash_search(
                        search_hash_half,
                        (hashtable_hash_half_atomic_t*)half_hashes_chunk->half_hashes,
                        skip_indexes_mask);

                LOG_DI(">>> chunk_slot_index = %lu", chunk_slot_index);

                if (chunk_slot_index == HASHTABLE_SUPPORT_HASH_SEARCH_NOT_FOUND) {
                    break;
                }

                // Update the skip indexes in case of another iteration
                skip_indexes_mask |= 1u << chunk_slot_index;

                LOG_DI(">>> skip_indexes_mask = 0x%08x", skip_indexes_mask);
                LOG_DI(">>> fetching key_value at bucket index = %lu",
                       (chunk_index * HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT) + chunk_slot_index);

                key_value = &hashtable_data->keys_values[
                        (chunk_index * HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT) + chunk_slot_index];

                LOG_DI(">>> key_value->flags = 0x%08x", key_value->flags);

                if (searching_or_creating == 0) {
                    if (HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags,
                                                     HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE)) {
                        LOG_DI(">>> key_value->flags has HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE");

                        found_key = key_value->inline_key.data;
                        found_key_max_compare_size = HASHTABLE_KEY_INLINE_MAX_LENGTH;
                    } else {
                        LOG_DI(">>> key_value->flags hasn't HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE");

#if defined(CACHEGRAND_HASHTABLE_KEY_CHECK_FULL)
                        LOG_DI(">>> CACHEGRAND_HASHTABLE_KEY_CHECK_FULL defined, comparing full key");

                        // TODO: The keys must be stored in an append only memory structure to avoid locking, memory can't
                        //       be freed immediately after the bucket is freed because it can be in use and would cause a
                        //       crash
                        found_key = key_value->external_key.data;
                        found_key_max_compare_size = key_value->external_key.size;

                        if (key_value->external_key.size != key_size) {
                            LOG_DI(">>> key have different length (%lu != %lu), continuing",
                                     key_size, key_value->external_key.size);
                            continue;
                        }
#else
                        LOG_DI(">>> CACHEGRAND_HASHTABLE_KEY_CHECK_FULL not defined, comparing only key prefix");

                        found_key = key_value->prefix_key.data;
                        found_key_max_compare_size = HASHTABLE_KEY_PREFIX_SIZE;

                        if (key_value->prefix_key.size != key_size) {
                            LOG_DI(">>> key have different length (%lu != %lu), continuing",
                                   key_size, key_value->prefix_key.size);
                            continue;
                        }
#endif // CACHEGRAND_HASHTABLE_KEY_CHECK_FULL
                    }

                    LOG_DI(">>> key fetched, comparing");

                    if (strncmp(key, (const char *)found_key, found_key_max_compare_size) != 0) {
                        char* temp_found_key = (char*)malloc(found_key_max_compare_size + 1);
                        temp_found_key[found_key_max_compare_size] = 0;
                        strncpy(temp_found_key, (char*)found_key, found_key_max_compare_size);

                        LOG_DI(">>> key different (%s != %s), unable to continue", key, found_key);
                        continue;
                    }

                    LOG_DI(">>> match found");
                } else {
                    // Not needed to perform a memory store barrier here, at some point the cpu will flush or a memory
                    // barrier will be issued. Setting the value without flushing is not impacting the functionalities.
                    half_hashes_chunk->half_hashes[chunk_slot_index] = hash_half;
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
                uint8_atomic_t overflowed_chunks_counter_new = (uint8_t)(found_chunk_index - chunk_index_start_initial);
                uint8_atomic_t overflowed_chunks_counter_current =
                        hashtable_data->half_hashes_chunk[chunk_index_start_initial].metadata.overflowed_chunks_counter;
                uint8_atomic_t overflowed_chunks_counter_update = max(
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

    LOG_DI("unlocking chunks from %lu to %lu", chunk_index_start_initial, locked_up_to_chunk_index);
    for (chunk_index = chunk_index_start_initial; chunk_index <= locked_up_to_chunk_index; chunk_index++) {
        LOG_DI("> processing chunk %lu", chunk_index);
        if (found == true && chunk_index == found_chunk_index) {
            LOG_DI("> chunk to return to the caller, keeping it locked");
            continue;
        }
        half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index];

        LOG_DI("> unlocking chunk");
        concat(hashtable_support_op_half_hashes_chunk_unlock, CACHEGRAND_HASHTABLE_SUPPORT_OP_ARCH_SUFFIX)(half_hashes_chunk);
    }

    LOG_DI("found_half_hashes_chunk = 0x%016x", *found_half_hashes_chunk);
    LOG_DI("found_key_value = 0x%016x", *found_key_value);
    LOG_DI("found = %s", found ? "YES" : "NO");

    return found;
}
