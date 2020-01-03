#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>

#include "xalloc.h"

#include "hashtable.h"
#include "hashtable_support.h"
#include "hashtable_gc.h"
#include "hashtable_op_set.h"

bool hashtable_search_key_or_create_new(
        volatile hashtable_data_t* hashtable_data,
        hashtable_key_data_t* key,
        hashtable_key_size_t key_size,
        hashtable_bucket_hash_t hash,
        bool* created_new,
        hashtable_bucket_index_t* found_index,
        volatile hashtable_bucket_key_value_t** found_key_value) {
    volatile hashtable_key_data_t* found_bucket_key;
    hashtable_key_size_t found_bucket_key_max_compare_size;
    hashtable_bucket_index_t index_neighborhood_begin, index_neighborhood_end;
    bool found = false;

    hashtable_calculate_neighborhood(
            hashtable_data->buckets_count,
            hash,
            &index_neighborhood_begin,
            &index_neighborhood_end);

    // TODO: the implementation below is buggy, potentially two or more thread can create the same key. A possible
    //       way to resolve this is to set the key and then loop over the neighborhood again to ensure that there are
    //       no other keys with the same value and if they are found then the key closer to the begin is kept.
    //       The problem with this approach is that the check should be performed always and after the key is created
    //       and this may potentially cause a resize.
    //       The possibility of a resize, though, is very unlikely because the neighborhood is fairly huge so unless
    //       there is a massive amount request to set a bucket with the same key.
    //       --- Need to investigate it further ---

    bool terminate_outer_loop = false;
    for(uint8_t searching_or_creating = 0; searching_or_creating < 2; searching_or_creating++) {

        HASHTABLE_MEMORY_FENCE_LOAD();

        for(hashtable_bucket_index_t index = index_neighborhood_begin; index < index_neighborhood_end; index++) {
            if (searching_or_creating == 0) {
                // If it's searching, loop of the neighborhood searching for the hash
                if (hashtable_data->hashes[index] != hash) {
                    continue;
                }
            } else {
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
                    if (expected_hash != hash) {
                        continue;
                    }
                }
            }

            volatile hashtable_bucket_key_value_t* found_bucket_key_value = &hashtable_data->keys_values[index];

            if (searching_or_creating == 0 || *created_new == false) {
                if (
                        HASHTABLE_BUCKET_KEY_VALUE_IS_EMPTY(found_bucket_key_value->flags)
                        ||
                        HASHTABLE_BUCKET_KEY_VALUE_HAS_FLAG(
                                found_bucket_key_value->flags,
                                HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED)) {
                    terminate_outer_loop = true;
                    break;
                }

                if (HASHTABLE_BUCKET_KEY_VALUE_HAS_FLAG(
                        found_bucket_key_value->flags,
                        HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE)) {
                    found_bucket_key = (volatile hashtable_key_data_t*)&found_bucket_key_value->inline_key.data;
                    found_bucket_key_max_compare_size = HASHTABLE_INLINE_KEY_MAX_SIZE;
                } else {
                    found_bucket_key = found_bucket_key_value->external_key.data;
                    found_bucket_key_max_compare_size = found_bucket_key_value->external_key.size;
                }

                int res = strncmp(key, (const char *)found_bucket_key, MIN(found_bucket_key_max_compare_size, key_size));
                if (res != 0) {
                    continue;
                }
            }

            *found_index = index;
            *found_key_value = found_bucket_key_value;
            found = true;
            break;
        }

        HASHTABLE_MEMORY_FENCE_STORE();

        if (terminate_outer_loop) {
            break;
        }
    }

    return found;
}

bool hashtable_set(hashtable_t* hashtable, hashtable_key_data_t* key, hashtable_key_size_t key_size, void* value) {
    bool bucket_found, created_new;
    hashtable_bucket_index_t bucket_index;
    hashtable_bucket_hash_t hash;
    hashtable_key_data_t* ht_key;
    bool cleaned_up = false;
    bool resized = false;
    volatile hashtable_bucket_key_value_t* bucket_key_value;

    hash = hashtable_calculate_hash(key, key_size);

    do {

        bucket_found = hashtable_search_key_or_create_new(
                hashtable->ht_current,
                key,
                key_size,
                hash,
                &created_new,
                &bucket_index,
                &bucket_key_value);

        // TODO: review, bucket_found == false is set as well when the hash is found but the bucket is still empty!
        if (bucket_found == false) {
            if (cleaned_up == false) {
                hashtable_garbage_collect_neighborhood(hashtable, bucket_index);
                cleaned_up = true;
                continue;
            }

            if (resized == false && hashtable->config->can_auto_resize) {
                // TODO: implement (auto)resize
                // hashhtable_scale_up_start(hashtable);
                resized = true;
                continue;
            }

            break;
        }
    }
    while(bucket_found == false);

    // Unable to find any bucket available and not able to auto resize (ie. max hashtable size or auto resize disabled)
    if (bucket_found == false) {
        return false;
    }

    if (created_new) {
        // It's a new bucket, the other threads will not access the bucket till the flags will be set to != 0
        bucket_key_value->data.void_data = value;
    } else {
        // Update / Set the data in an atomic way with a CAS, something else may have changed it in the mean time!
        hashtable_value_data_t value_data_temp;
        value_data_temp.void_data = value;
        uintptr_t current_data = bucket_key_value->data.uintptr_data;
        if (atomic_compare_exchange_strong(
                &bucket_key_value->data.uintptr_data,
                &current_data,
                value_data_temp.uintptr_data) == false) {

        }
    }

    HASHTABLE_MEMORY_FENCE_LOAD_STORE();

    // Check if it's a new bucket or not
    if (created_new) {
        // Get the destination pointer for the key
        if (key_size <= HASHTABLE_INLINE_KEY_MAX_SIZE) {
            ht_key = (hashtable_key_data_t *)&bucket_key_value->inline_key.data;
        } else {
            ht_key = xalloc(key_size + 1);
            ht_key[key_size] = '\0';

            bucket_key_value->external_key.data = ht_key;
            bucket_key_value->external_key.size = key_size;
        }

        // Copy the key
        strncpy((char*)ht_key, key, key_size);

        // Set the FILLED flag (drops the deleted flag as well)
        bucket_key_value->flags = HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED;
    }

    return bucket_found;
}
