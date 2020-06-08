#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <sched.h>

#include "xalloc.h"
#include "memory_fences.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_index.h"
#include "hashtable/hashtable_support_op.h"
#include "hashtable/hashtable_support_hash_search.h"

// TODO: refactor to merge the functions hashtable_support_op_search_key and
//       hashtable_support_op_search_key_or_create_new and reorganize the code

bool hashtable_support_op_search_key(
        volatile hashtable_data_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_bucket_hash_t hash,
        hashtable_bucket_hash_half_t hash_half,
        volatile hashtable_bucket_t **found_bucket,
        hashtable_bucket_index_t *found_bucket_index,
        hashtable_bucket_slot_index_t *found_bucket_slot_index,
        volatile hashtable_bucket_key_value_t **found_bucket_key_value) {
    volatile hashtable_bucket_t* bucket;
    hashtable_bucket_index_t bucket_index;
    hashtable_bucket_slot_index_t bucket_slot_index;
    volatile hashtable_bucket_key_value_t* bucket_key_value;
    volatile hashtable_key_data_t* found_bucket_key;
    hashtable_key_size_t found_bucket_key_max_compare_size;
    uint32_t skip_indexes_mask;
    bool found = false;

    bucket_index = hashtable_support_index_from_hash(hashtable_data->buckets_count, hash);

    HASHTABLE_MEMORY_FENCE_LOAD();

    bucket = &hashtable_data->buckets[bucket_index];

#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
    if (bucket->keys_values == NULL) {
        return found;
    }
#endif // HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0


    skip_indexes_mask = 0;

    // TODO: with AVX/AVX2 I can get a full match on all the results after the first iteration, no need to re-invoke
    //       the search function again! The search function should return the bitmask instead of having to re-run
    //       it entirely every single time
    HASHTABLE_MEMORY_FENCE_LOAD();
    while((bucket_slot_index = hashtable_support_hash_search(
            hash_half,
            (hashtable_bucket_hash_half_atomic_t*)bucket->half_hashes,
            skip_indexes_mask)) != HASHTABLE_SUPPORT_HASH_SEARCH_NOT_FOUND) {

        // Update the skip indexes in case of another iteration
        skip_indexes_mask |= 1u << bucket_slot_index;

        bucket_key_value = &bucket->keys_values[bucket_slot_index];

        // Stop if hash found but bucket being filled, edge case because of parallelism, if marked as delete continue
        // and if key doesn't match continue as well. The order of the operations doesn't matter.
        if (HASHTABLE_BUCKET_KEY_VALUE_IS_EMPTY(bucket_key_value->flags)) {
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

        *found_bucket = bucket;
        *found_bucket_index = bucket_index;
        *found_bucket_slot_index = bucket_slot_index;
        *found_bucket_key_value = bucket_key_value;
        found = true;
        break;
    }

    return found;
}

#if HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1
bool hashtable_support_op_bucket_lock(
        volatile hashtable_bucket_t* bucket,
        bool retry) {
    bool write_lock_set = false;

    if (retry) {
        do {
            uint128_t expected_value = 0;

            write_lock_set = __sync_bool_compare_and_swap(
                    &bucket->write_lock,
                    expected_value,
                    1);

            if (!write_lock_set) {
                sched_yield();
            }
        } while(!write_lock_set);
    } else {
        uint128_t expected_value = 0;

        write_lock_set = __sync_bool_compare_and_swap(
                &bucket->write_lock,
                expected_value,
                1);
    }

    return write_lock_set;
}

void hashtable_support_op_bucket_unlock(
        volatile hashtable_bucket_t* bucket) {
    bucket->write_lock = 0;
    HASHTABLE_MEMORY_FENCE_STORE();
}
#endif // HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1

#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
hashtable_bucket_key_value_t* hashtable_support_op_bucket_alloc_keys_values(
        volatile hashtable_bucket_t* bucket) {
    hashtable_bucket_key_value_t* keys_values;
    keys_values = (hashtable_bucket_key_value_t*)xalloc_alloc(sizeof(hashtable_bucket_key_value_t) * HASHTABLE_BUCKET_SLOTS_COUNT);
    memset(keys_values, 0, sizeof(hashtable_bucket_key_value_t) * HASHTABLE_BUCKET_SLOTS_COUNT);

    return keys_values;
}
#endif // HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0

#if HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1 || HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
volatile hashtable_bucket_t* hashtable_support_op_bucket_fetch_and_write_lock(
        volatile hashtable_data_t *hashtable_data,
        hashtable_bucket_index_t bucket_index,
        bool initialize_new_if_missing,
        bool *initialized) {
    volatile hashtable_bucket_t* bucket;

    HASHTABLE_MEMORY_FENCE_LOAD();

    bucket = &hashtable_data->buckets[bucket_index];

#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
    if ((bucket->keys_values == NULL && initialize_new_if_missing == false)) {
        return NULL;
    }
#endif

#if HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1
    hashtable_support_op_bucket_lock(bucket, true);
#endif // HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1

#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
    if (bucket->keys_values == NULL) {
        bucket->keys_values = hashtable_support_op_bucket_alloc_keys_values(bucket);
        HASHTABLE_MEMORY_FENCE_STORE();
        *initialized = true;
    }
#endif

    return bucket;
}
#endif

bool hashtable_support_op_search_key_or_create_new(
        volatile hashtable_data_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_bucket_hash_t hash,
        hashtable_bucket_hash_half_t hash_half,
        bool create_new_if_missing,
        bool *created_new,
        volatile hashtable_bucket_t **found_bucket,
        hashtable_bucket_index_t *found_bucket_index,
        hashtable_bucket_slot_index_t *found_bucket_slot_index,
        volatile hashtable_bucket_key_value_t **found_bucket_key_value) {
    hashtable_bucket_hash_half_t search_hash_half;
    volatile hashtable_bucket_t* bucket;
    hashtable_bucket_index_t bucket_index;
    hashtable_bucket_slot_index_t bucket_slot_index;
    volatile hashtable_bucket_key_value_t* bucket_key_value;
    volatile hashtable_key_data_t* found_bucket_key;
    hashtable_key_size_t found_bucket_key_max_compare_size;
    uint32_t skip_indexes_mask;

    bool bucket_newly_initialized = false;
    bool found = false;

    bucket_index = hashtable_support_index_from_hash(hashtable_data->buckets_count, hash);

#if HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1 || HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
    bucket = hashtable_support_op_bucket_fetch_and_write_lock(
            hashtable_data,
            bucket_index,
            create_new_if_missing,
            &bucket_newly_initialized);

#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
    if (bucket == NULL) {
        return false;
    }
#endif // HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
#else
    HASHTABLE_MEMORY_FENCE_LOAD();
    bucket = &hashtable_data->buckets[bucket_index];
#endif

    // The lock has to be removed so the bucket must be flagged as found
    *found_bucket = bucket;
    *found_bucket_index = bucket_index;

    for(
            uint8_t searching_or_creating = 0;
            (searching_or_creating < (create_new_if_missing ? 2 : 1)) && found == false;
            searching_or_creating++) {

        if (searching_or_creating == 0) {
            search_hash_half = hash_half;
        } else {
            search_hash_half = 0;
        }

        skip_indexes_mask = 0;

        // TODO: to have a proper non locking code hash_search has to return the mask with the matches, it has to be
        //       compared with a previous fetched value and, only if not changed, then the skip indexes mask can be
        //       applied and the bucket_slot_index calculated.

        while ((bucket_slot_index = hashtable_support_hash_search(
                search_hash_half,
                (hashtable_bucket_hash_half_atomic_t *) bucket->half_hashes,
                skip_indexes_mask)) != HASHTABLE_SUPPORT_HASH_SEARCH_NOT_FOUND) {

            // Update the skip indexes in case of another iteration
            skip_indexes_mask |= 1u << bucket_slot_index;


#if HASHTABLE_BUCKET_FEATURE_USE_LOCK == 0
            if (searching_or_creating == 1) {
                hashtable_bucket_hash_half_t expected_hash = 0U;

                // If the operation is successful, it's a new bucket, if it fails it may be an existing bucket, this
                // specific case is checked below
                *created_new = atomic_compare_exchange_strong(
                        &bucket->half_hashes[bucket_slot_index],
                        &expected_hash,
                        hash_half);

                if (*created_new == false) {
                    // Corner case, consider valid the operation if the new hash is what was going to be set
                    if (expected_hash != hash_half) {
                        continue;
                    }
                }
            }
#endif // HASHTABLE_BUCKET_FEATURE_USE_LOCK == 0
            bucket_key_value = &bucket->keys_values[bucket_slot_index];

#if HASHTABLE_BUCKET_FEATURE_USE_LOCK == 0
            if (searching_or_creating == 0 || (searching_or_creating == 1 && *created_new == false)) {
#else
            if (searching_or_creating == 0) {
#endif // HASHTABLE_BUCKET_FEATURE_USE_LOCK == 0
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
            }
#if HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1
            else {
                bucket->half_hashes[bucket_slot_index] = hash_half;
                *created_new = true;

                HASHTABLE_MEMORY_FENCE_STORE();
            }
#endif // HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1

            *found_bucket_slot_index = bucket_slot_index;
            *found_bucket_key_value = bucket_key_value;
            found = true;
            break;
        }
    }

    return found;
}
