#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <sched.h>

#include "xalloc.h"
#include "memory_fences.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_primenumbers.h"
#include "hashtable/hashtable_support_index.h"
#include "hashtable/hashtable_support_hash.h"
#include "hashtable/hashtable_support_op.h"
#include "hashtable/hashtable_support_hash_search.h"

// TODO: refactor to merge the functions hashtable_support_op_search_key and
//       hashtable_support_op_search_key_or_create_new and reorganize the code

bool hashtable_support_op_search_key(
        volatile hashtable_data_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_hash_t hash,
        volatile hashtable_bucket_key_value_t **found_bucket_key_value) {
    hashtable_hash_half_t hash_half;
    hashtable_bucket_index_t bucket_index;
    hashtable_chunk_index_t chunk_index, chunk_index_start, chunk_index_mod;
    hashtable_chunk_slot_index_t chunk_slot_index;
    hashtable_half_hashes_chunk_atomic_t * half_hashes_chunk;
    volatile hashtable_bucket_key_value_t* bucket_key_value;
    volatile hashtable_key_data_t* found_bucket_key;
    hashtable_key_size_t found_bucket_key_max_compare_size;
    uint32_t skip_indexes_mask;
    bool found = false;

    hash_half = hashtable_support_hash_half(hash);
    bucket_index = hashtable_support_index_from_hash(hashtable_data->buckets_count, hash);
    chunk_index_start = bucket_index / HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT;

    HASHTABLE_MEMORY_FENCE_LOAD();

    half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index_start];
    uint8_atomic_t overflowed_chunks_counter = half_hashes_chunk->metadata.overflowed_chunks_counter;

    for(
            chunk_index = chunk_index_start;
            chunk_index < chunk_index_start + overflowed_chunks_counter && found == false;
            chunk_index++) {
        chunk_index_mod = hashtable_support_primenumbers_mod(chunk_index, hashtable_data->buckets_count);
        half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index_mod];

        skip_indexes_mask = 0;

        while(true) {
            HASHTABLE_MEMORY_FENCE_LOAD();

            chunk_slot_index = hashtable_support_hash_search(
                hash_half,
                (hashtable_hash_half_atomic_t*)half_hashes_chunk->half_hashes,
                skip_indexes_mask);

            if (chunk_slot_index == HASHTABLE_SUPPORT_HASH_SEARCH_NOT_FOUND) {
                break;
            }

            // Update the skip indexes in case of another iteration
            skip_indexes_mask |= 1u << chunk_slot_index;

            bucket_key_value = &hashtable_data->keys_values[
                    (chunk_index * HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT) + chunk_slot_index];

            // Stop if hash found but bucket being filled, edge case because of parallelism.
            if (HASHTABLE_BUCKET_KEY_VALUE_IS_EMPTY(bucket_key_value->flags)) {
                break;
            } else if (HASHTABLE_BUCKET_KEY_VALUE_HAS_FLAG(bucket_key_value->flags,
                                                           HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED)) {
                continue;
            } else {
                // The key may potentially change if the item is first deleted and then recreated, if it's inline it
                // doesn't really matter because the key will mismatch and the execution will continue but if the key is
                // stored externally and the allocated memory is freed it may crash.
                if (HASHTABLE_BUCKET_KEY_VALUE_HAS_FLAG(bucket_key_value->flags,
                                                        HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE)) {
                    found_bucket_key = bucket_key_value->inline_key.data;
                    found_bucket_key_max_compare_size = HASHTABLE_KEY_INLINE_MAX_LENGTH;
                } else {
    #if defined(CACHEGRAND_HASHTABLE_KEY_CHECK_FULL)
                    // TODO: The keys must be stored in an append only memory structure to avoid locking, memory can't
                    //       be freed immediately after the bucket is freed because it can be in use and would cause a
                    //       crash
                    found_bucket_key = bucket_key_value->external_key.data;
                    found_bucket_key_max_compare_size = bucket_key_value->external_key.size;

                    if (bucket_key_value->external_key.size != key_size) {
                        continue;
                    }
    #else
                    found_bucket_key = bucket_key_value->prefix_key.data;
                    found_bucket_key_max_compare_size = HASHTABLE_KEY_PREFIX_SIZE;

                    if (bucket_key_value->prefix_key.size != key_size) {
                        continue;
                    }
    #endif // CACHEGRAND_HASHTABLE_KEY_CHECK_FULL
                }

                if (
                        (HASHTABLE_BUCKET_KEY_VALUE_HAS_FLAG(
                            bucket_key_value->flags,
                            HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED))
                        ||
                        (strncmp(
                            key,
                            (const char *) found_bucket_key,
                            found_bucket_key_max_compare_size) != 0)) {
                    continue;
                }
            }

            *found_bucket_key_value = bucket_key_value;
            found = true;
            break;
        }
    }

    return found;
}

bool hashtable_support_op_half_hashes_chunk_lock(
        hashtable_half_hashes_chunk_atomic_t * half_hashes_chunk,
        bool retry) {
    bool write_lock_set = false;

    // The code is duplicated but it
    do {
        uint8_t expected_value = 0;

        write_lock_set = __sync_bool_compare_and_swap(
                &half_hashes_chunk->metadata.write_lock,
                expected_value,
                1);

        if (!write_lock_set) {
            sched_yield();
        }
    } while(retry && !write_lock_set);

    return write_lock_set;
}

void hashtable_support_op_half_hashes_chunk_unlock(
        hashtable_half_hashes_chunk_atomic_t* half_hashes_chunk) {
    half_hashes_chunk->metadata.write_lock = 0;
    HASHTABLE_MEMORY_FENCE_STORE();
}

bool hashtable_support_op_search_key_or_create_new(
        hashtable_data_atomic_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_hash_t hash,
        bool create_new_if_missing,
        bool *created_new,
        hashtable_half_hashes_chunk_atomic_t **found_half_hashes_chunk,
        volatile hashtable_bucket_key_value_t **found_bucket_key_value) {
    hashtable_hash_half_t hash_half;
    hashtable_hash_half_t search_hash_half;
    hashtable_bucket_index_t bucket_index;
    hashtable_chunk_index_t chunk_index, chunk_index_start, chunk_index_end, chunk_index_mod,
        chunk_first_with_freespace, found_chunk_index = 0, locked_up_to_chunk_index = 0;
    hashtable_chunk_slot_index_t chunk_slot_index;
    hashtable_half_hashes_chunk_atomic_t* half_hashes_chunk;
    volatile hashtable_bucket_key_value_t* bucket_key_value;
    volatile hashtable_key_data_t* found_bucket_key;
    hashtable_key_size_t found_bucket_key_max_compare_size;
    uint32_t skip_indexes_mask;
    bool found = false;
    bool found_chunk_with_freespace = false;

    hash_half = hashtable_support_hash_half(hash);
    bucket_index = hashtable_support_index_from_hash(hashtable_data->buckets_count, hash);
    chunk_index_start = bucket_index / HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT;

    half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index_start];

    hashtable_support_op_half_hashes_chunk_lock(half_hashes_chunk, true);
    locked_up_to_chunk_index = chunk_index_start;

    uint8_atomic_t overflowed_chunks_counter = half_hashes_chunk->metadata.overflowed_chunks_counter;

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
                    (bucket_index / HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT) +
                    (chunk_first_with_freespace - chunk_index_start) +
                    HASHTABLE_HALF_HASHES_CHUNK_SEARCH_MAX;
        }

        // Setup the search half hash
        if (searching_or_creating == 0) {
            search_hash_half = hash_half;
        } else {
            search_hash_half = 0;
        }

        for (
                chunk_index = chunk_index_start;
                chunk_index < chunk_index_end && found == false;
                chunk_index++) {
            chunk_index_mod = hashtable_support_primenumbers_mod(chunk_index, hashtable_data->buckets_count);
            half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index_mod];

            // Every time a new chunk gets processed during the search it has to be locked for safety reasons
            if (chunk_index > locked_up_to_chunk_index) {
                hashtable_support_op_half_hashes_chunk_lock(half_hashes_chunk, true);
            }

            if (searching_or_creating == 0) {
                // Check if it has found a chunk with free space
                if (found_chunk_with_freespace == false) {
                    chunk_first_with_freespace = chunk_index;

                    if (half_hashes_chunk->metadata.is_full == 0) {
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

                if (chunk_slot_index == HASHTABLE_SUPPORT_HASH_SEARCH_NOT_FOUND) {
                    break;
                }

                // Update the skip indexes in case of another iteration
                skip_indexes_mask |= 1u << chunk_slot_index;

                bucket_key_value = &hashtable_data->keys_values[
                        (chunk_index * HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT) + chunk_slot_index];

                if (searching_or_creating == 0) {
                    if (HASHTABLE_BUCKET_KEY_VALUE_HAS_FLAG(bucket_key_value->flags,
                                                            HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE)) {
                        found_bucket_key = bucket_key_value->inline_key.data;
                        found_bucket_key_max_compare_size = HASHTABLE_KEY_INLINE_MAX_LENGTH;
                    } else {
#if defined(CACHEGRAND_HASHTABLE_KEY_CHECK_FULL)
                        // TODO: The keys must be stored in an append only memory structure to avoid locking, memory can't
                    //       be freed immediately after the bucket is freed because it can be in use and would cause a
                    //       crash
                    found_bucket_key = bucket_key_value->external_key.data;
                    found_bucket_key_max_compare_size = bucket_key_value->external_key.size;

                    if (bucket_key_value->external_key.size != key_size) {
                        continue;
                    }
#else
                        found_bucket_key = bucket_key_value->prefix_key.data;
                        found_bucket_key_max_compare_size = HASHTABLE_KEY_PREFIX_SIZE;

                        if (bucket_key_value->prefix_key.size != key_size) {
                            continue;
                        }
#endif // CACHEGRAND_HASHTABLE_KEY_CHECK_FULL
                    }

                    if (strncmp(key, (const char *)found_bucket_key, found_bucket_key_max_compare_size) != 0) {
                        continue;
                    }
                } else {
                    // Not needed to perform a memory store barrier here, at some point the cpu will flush or a memory
                    // barrier will be issued. Setting the value without flushing is not impacting the functionalities.
                    half_hashes_chunk->half_hashes[chunk_slot_index] = hash_half;
                    *created_new = true;
                }

                *found_bucket_key_value = bucket_key_value;
                found_chunk_index = chunk_index;
                found = true;
                break;
            }

            if (searching_or_creating == 1) {
                if (found == false) {
                    // Not needed to perform a memory store barrier here, the is_full metadata is used only by the
                    // the thread holding the lock
                    half_hashes_chunk->metadata.is_full = 1;
                }
            }
        }
    }

    // Iterate of the chunks to remove the place locks, the only lock not removed is if the chunk holding the hash
    for (
            chunk_index = bucket_index / HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT;
            chunk_index < locked_up_to_chunk_index;
            chunk_index++) {
        if (found == true && chunk_index == found_chunk_index) {
            continue;
        }
        chunk_index_mod = hashtable_support_primenumbers_mod(chunk_index, hashtable_data->buckets_count);
        half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index_mod];

        hashtable_support_op_half_hashes_chunk_unlock(half_hashes_chunk);
    }

    return found;
}
