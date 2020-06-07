#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>

#include "xalloc.h"
#include "hashtable.h"
#include "hashtable_support_index.h"
#include "hashtable_support_op.h"
#include "hashtable_support_hash.h"
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

hashtable_search_key_or_create_new_ret_t hashtable_support_op_search_key_or_create_new(
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
                &expected_value,
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
        bool create_new_if_missing) {
    volatile hashtable_bucket_t* bucket;
    bool created_new = false;

    // Ensure that the initial chain ring already exists
    do {
        HASHTABLE_MEMORY_FENCE_LOAD();

        bucket = &hashtable_data->buckets[bucket_index];

        if (bucket->chain_first_ring != NULL || (bucket->chain_first_ring == NULL && create_new_if_missing == false)) {
            break;
        }

        if (bucket->write_lock == 1) {
            // TODO: sched_yield
            continue;
        }

        // Try to lock the bucket for writes
        if (hashtable_support_op_bucket_lock(bucket, false)) {
            continue;
        }

        hashtable_bucket_chain_ring_t* chain_first_ring;
        chain_first_ring = (hashtable_bucket_chain_ring_t*)xalloc_alloc(sizeof(hashtable_bucket_chain_ring_t));
        memset(chain_first_ring, 0, sizeof(hashtable_bucket_chain_ring_t));

        bucket->chain_first_ring = chain_first_ring;
        HASHTABLE_MEMORY_FENCE_STORE();

        created_new = true;

        break;
    } while(true);

    if (bucket->chain_first_ring == NULL) {
        return NULL;
    }

    if (created_new == false) {
        hashtable_support_op_bucket_lock(bucket, true);
    }

    return bucket;
}

        volatile hashtable_data_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_bucket_hash_t hash,
        bool *created_new,
        hashtable_bucket_index_t *found_index,
        volatile hashtable_bucket_key_value_t **found_key_value) {
    volatile hashtable_key_data_t* found_bucket_key;
    hashtable_key_size_t found_bucket_key_max_compare_size;
    hashtable_bucket_index_t index_neighborhood_begin, index_neighborhood_end;
    hashtable_search_key_or_create_new_ret_t ret = HASHTABLE_SEARCH_KEY_OR_CREATE_NEW_RET_NO_FREE;

    hashtable_support_index_calculate_neighborhood_from_hash(
            hashtable_data->buckets_count,
            hash,
            hashtable_data->cachelines_to_probe,
            &index_neighborhood_begin,
            &index_neighborhood_end);

    bool terminate_outer_loop = false;
    for(uint8_t searching_or_creating = 0; searching_or_creating < 2; searching_or_creating++) {
        for(hashtable_bucket_index_t index = index_neighborhood_begin; index <= index_neighborhood_end; index++) {
            HASHTABLE_MEMORY_FENCE_LOAD();

            if (searching_or_creating == 0) {
                // If it's searching, loop of the neighborhood searching for the hash
                if (hashtable_data->hashes[index] != hash) {
                    continue;
                }

            } else if (searching_or_creating == 1) {
                // If it's creating, it has still to search not only an empty bucket but a bucket with the key as well
                // because it may have been created in the mean time
                if (hashtable_data->hashes[index] != hash && hashtable_data->hashes[index] != 0) {
                    continue;
                }

                hashtable_bucket_hash_t expected_hash = 0U;

                // If the operation is successful, it's a new bucket, if it fails it may be an existing bucket, this
                // specific case is checked below
                *created_new = atomic_compare_exchange_strong(&hashtable_data->hashes[index], &expected_hash, hash);

                if (*created_new == false) {
                    // Corner case, consider valid the operation if the new hash is what was going to be set
                    if (expected_hash != hash) {
                        continue;
                    }
                }
            }

            volatile hashtable_bucket_key_value_t* found_bucket_key_value = &hashtable_data->keys_values[index];

            if (searching_or_creating == 0 || *created_new == false) {
                // The flags are set as very last information so it's safe to assume that if they are empty there is a
                // set that is creating a bucket ran by another thread that hasn't finished yet.
                // If it's marked as deleted instead, it means that the bucket was containing the right hash but has
                // been GCed or deleted and there fore it's pointless to set the new value because the intent was to
                // remove this specific key/value.
                if (
                        HASHTABLE_BUCKET_KEY_VALUE_IS_EMPTY(found_bucket_key_value->flags)
                        ||
                        HASHTABLE_BUCKET_KEY_VALUE_HAS_FLAG(
                                found_bucket_key_value->flags,
                                HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED)) {
                    terminate_outer_loop = true;
                    ret = HASHTABLE_SEARCH_KEY_OR_CREATE_NEW_RET_EMPTY_OR_DELETED;
                    break;
                }

                if (HASHTABLE_BUCKET_KEY_VALUE_HAS_FLAG(
                        found_bucket_key_value->flags,
                        HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE)) {
                    found_bucket_key = (volatile hashtable_key_data_t*)&found_bucket_key_value->inline_key.data;
                    found_bucket_key_max_compare_size = HASHTABLE_KEY_INLINE_MAX_LENGTH;
                } else {
                    found_bucket_key = found_bucket_key_value->external_key.data;
                    found_bucket_key_max_compare_size = found_bucket_key_value->external_key.size;
                }

                int res = strncmp(key, (const char *)found_bucket_key, MIN(found_bucket_key_max_compare_size, key_size));
                if (res != 0) {
                    continue;
                }
            }

            HASHTABLE_MEMORY_FENCE_STORE();

            *found_index = index;
            *found_key_value = found_bucket_key_value;
            ret = HASHTABLE_SEARCH_KEY_OR_CREATE_NEW_RET_FOUND;
            terminate_outer_loop = true;
            break;
        }

        if (terminate_outer_loop) {
            break;
        }
    }

    return ret;
}
