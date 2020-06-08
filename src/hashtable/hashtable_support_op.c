#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <sched.h>

#include "xalloc.h"
#include "hashtable.h"
#include "hashtable_support_index.h"
#include "hashtable_support_op.h"
#include "hashtable_support_hash_search.h"

// TODO: refactor to merge the functions hashtable_support_op_search_key and
//       hashtable_support_op_search_key_or_create_new and reorganize the code

bool hashtable_support_op_search_key(
        volatile hashtable_data_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_bucket_hash_t hash,
        hashtable_bucket_hash_half_t hash_half,
        volatile hashtable_bucket_chain_ring_t **found_chain_ring,
        hashtable_bucket_chain_ring_index_t *found_chain_ring_index,
        volatile hashtable_bucket_key_value_t **found_bucket_key_value) {
    volatile hashtable_key_data_t* found_bucket_key;
    volatile hashtable_bucket_t* bucket;
    hashtable_key_size_t found_bucket_key_max_compare_size;
    hashtable_bucket_index_t bucket_index;
    hashtable_bucket_chain_ring_index_t ring_index;
    volatile hashtable_bucket_key_value_t* bucket_key_value;
    uint32_t skip_indexes_mask;
    bool found = false;
    bool stop_search = false;

    bucket_index = hashtable_support_index_from_hash(hashtable_data->buckets_count, hash);

    HASHTABLE_MEMORY_FENCE_LOAD();

    bucket = &hashtable_data->buckets[bucket_index];

    if (bucket == NULL || bucket->chain_first_ring == NULL) {
        return found;
    }

    volatile hashtable_bucket_chain_ring_t *ring = bucket->chain_first_ring;

    do {
        skip_indexes_mask = 0;
        stop_search = false;

        // TODO: with AVX/AVX2 I can get a full match on all the results after the first iteration, no need to re-invoke
        //       the search function again! The search function should return the bitmask instead of having to re-run
        //       it entirely every single time
        HASHTABLE_MEMORY_FENCE_LOAD();
        while((ring_index = hashtable_support_hash_search(
                hash_half,
                (hashtable_bucket_hash_half_atomic_t*)ring->half_hashes,
                skip_indexes_mask)) != HASHTABLE_SUPPORT_HASH_SEARCH_NOT_FOUND) {

            // Update the skip indexes in case of another iteration
            skip_indexes_mask |= 1u << ring_index;

            bucket_key_value = &ring->keys_values[ring_index];

            // Stop if hash found but bucket being filled, edge case because of parallelism, if marked as delete continue
            // and if key doesn't match continue as well. The order of the operations doesn't matter.
            if (HASHTABLE_BUCKET_KEY_VALUE_IS_EMPTY(bucket_key_value->flags)) {
                stop_search = true;
                break;
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

            *found_chain_ring = ring;
            *found_chain_ring_index = ring_index;
            *found_bucket_key_value = bucket_key_value;
            found = true;
            break;
        }
    } while(found == false && stop_search == false && (ring = ring->next_ring) != NULL);

    return found;
}

bool hashtable_support_op_bucket_lock(
        volatile hashtable_bucket_t* bucket,
        bool retry) {
    bool write_lock_set = false;

    uint128_t new_value = HASHTABLE_BUCKET_WRITE_LOCK_SET(bucket->_internal_cmpandxcg);

    // Acquire the lock on the bucket
    do {
        uint128_t expected_value = HASHTABLE_BUCKET_WRITE_LOCK_CLEAR(bucket->_internal_cmpandxcg);

        write_lock_set = __sync_bool_compare_and_swap(
                &bucket->_internal_cmpandxcg,
                expected_value,
                new_value);
    } while(!write_lock_set && retry == true);

    return write_lock_set;
}

void hashtable_support_op_bucket_unlock(
        volatile hashtable_bucket_t* bucket) {
    bucket->write_lock = 0;
    HASHTABLE_MEMORY_FENCE_STORE();
}

volatile hashtable_bucket_t* hashtable_support_op_bucket_fetch_and_write_lock(
        volatile hashtable_data_t *hashtable_data,
        hashtable_bucket_index_t bucket_index,
        bool initialize_new_if_missing,
        bool *initialized) {
    volatile hashtable_bucket_t* bucket;

    // Ensure that the initial chain ring already exists
    do {
        HASHTABLE_MEMORY_FENCE_LOAD();

        bucket = &hashtable_data->buckets[bucket_index];

        // Try to lock the bucket for writes
        if (hashtable_support_op_bucket_lock(bucket, false) == false) {
            sched_yield();
            continue;
        }

        if (
                bucket->chain_first_ring != NULL ||
                (bucket->chain_first_ring == NULL && initialize_new_if_missing == false)) {

            if (initialize_new_if_missing == false) {
                hashtable_support_op_bucket_unlock(bucket);
            }

            break;
        }

        hashtable_bucket_chain_ring_t* chain_first_ring;
        chain_first_ring = (hashtable_bucket_chain_ring_t*)xalloc_alloc(sizeof(hashtable_bucket_chain_ring_t));
        memset(chain_first_ring, 0, sizeof(hashtable_bucket_chain_ring_t));

        bucket->chain_first_ring = chain_first_ring;
        HASHTABLE_MEMORY_FENCE_STORE();

        *initialized = true;
    } while(*initialized == false);

    if (bucket->chain_first_ring == NULL) {
        return NULL;
    }

    return bucket;
}

bool hashtable_support_op_search_key_or_create_new(
        volatile hashtable_data_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_bucket_hash_t hash,
        hashtable_bucket_hash_half_t hash_half,
        bool create_new_if_missing,
        bool *created_new,
        volatile hashtable_bucket_t **found_bucket,
        volatile hashtable_bucket_chain_ring_t **found_chain_ring,
        hashtable_bucket_chain_ring_index_t *found_chain_ring_index,
        volatile hashtable_bucket_key_value_t **found_bucket_key_value) {
    hashtable_bucket_hash_half_t search_hash_half;
    volatile hashtable_key_data_t* found_bucket_key;
    hashtable_key_size_t found_bucket_key_max_compare_size;
    hashtable_bucket_index_t bucket_index;
    volatile hashtable_bucket_t* bucket;
    uint32_t skip_indexes_mask;
    volatile hashtable_bucket_key_value_t* bucket_key_value;
    hashtable_bucket_chain_ring_index_t ring_index;
    volatile hashtable_bucket_chain_ring_t
        *chain_ring = NULL,
        *chain_ring_first_with_free_slots = NULL,
        *chain_ring_last = NULL;

    bool bucket_newly_initialized = false;
    bool found = false;

    bucket_index = hashtable_support_index_from_hash(hashtable_data->buckets_count, hash);

    bucket = hashtable_support_op_bucket_fetch_and_write_lock(
            hashtable_data,
            bucket_index,
            create_new_if_missing,
            &bucket_newly_initialized);

    if (bucket == NULL) {
        return false;
    }

    *found_bucket = bucket;

    for(
            uint8_t searching_or_creating = 0;
            (searching_or_creating < (create_new_if_missing ? 2 : 1)) && found == false;
            searching_or_creating++) {
        if (searching_or_creating == 0) {
            chain_ring = bucket->chain_first_ring;
            search_hash_half = hash_half;
        } else {
            chain_ring = chain_ring_first_with_free_slots;
            search_hash_half = 0;
        }

        do {
            skip_indexes_mask = 0;

            if (searching_or_creating == 0) {
                chain_ring_last = chain_ring;

                if (
                        chain_ring_first_with_free_slots == NULL &&
                        (chain_ring->flags & HASHTABLE_BUCKET_CHAIN_RING_FLAG_FULL) == 0) {
                    chain_ring_first_with_free_slots = chain_ring;
                }
            }

            // If an empty slot is being searched in a chain ring and the chain_ring is full skip it
            if (
                    searching_or_creating == 1 &&
                    (chain_ring->flags & HASHTABLE_BUCKET_CHAIN_RING_FLAG_FULL) == HASHTABLE_BUCKET_CHAIN_RING_FLAG_FULL) {
                continue;
            }

            while ((ring_index = hashtable_support_hash_search(
                    search_hash_half,
                    (hashtable_bucket_hash_half_atomic_t *) chain_ring->half_hashes,
                    skip_indexes_mask)) != HASHTABLE_SUPPORT_HASH_SEARCH_NOT_FOUND) {

                // Update the skip indexes in case of another iteration
                skip_indexes_mask |= 1u << ring_index;

                bucket_key_value = &chain_ring->keys_values[ring_index];

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
                    chain_ring->half_hashes[ring_index] = hash_half;
                    *created_new = true;
                }

                *found_chain_ring = chain_ring;
                *found_chain_ring_index = ring_index;
                *found_bucket_key_value = bucket_key_value;
                found = true;
                break;
            }

            if (searching_or_creating == 1 && found == false) {
                chain_ring->flags |= HASHTABLE_BUCKET_CHAIN_RING_FLAG_FULL;
            }
        } while(found == false && (chain_ring = chain_ring->next_ring) != NULL);
    }

    // If an empty slot in one of the rings hasn't been found, a new ring gets created and the first slot assigned
    if (create_new_if_missing && found == false) {
        hashtable_bucket_chain_ring_t* chain_ring_next;
        chain_ring_next = (hashtable_bucket_chain_ring_t*)xalloc_alloc(sizeof(hashtable_bucket_chain_ring_t));
        memset(chain_ring_next, 0, sizeof(hashtable_bucket_chain_ring_t));
        chain_ring_last->next_ring = chain_ring_next;

        chain_ring_next->half_hashes[0] = hash_half;

        *found_chain_ring = chain_ring_next;
        *found_chain_ring_index = 0;
        *found_bucket_key_value = &chain_ring_next->keys_values[0];

        found = true;
        *created_new = true;
    }

    HASHTABLE_MEMORY_FENCE_STORE();

    return found;
}
