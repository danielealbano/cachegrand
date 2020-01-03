#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>

#include "hashtable.h"
#include "hashtable_support.h"
#include "hashtable_op_get.h"

bool hashtable_search_key(
        volatile hashtable_data_t* hashtable_data,
        hashtable_key_data_t* key,
        hashtable_key_size_t key_size,
        hashtable_bucket_hash_t hash,
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

    HASHTABLE_MEMORY_FENCE_LOAD();
    for(hashtable_bucket_index_t index = index_neighborhood_begin; index < index_neighborhood_end; index++) {
        if (hashtable_data->hashes[index] != hash) {
            continue;
        }

        volatile hashtable_bucket_key_value_t* found_bucket_key_value = &hashtable_data->keys_values[index];

        // The key may potentially change if the item is first deleted and then recreated, if it's inline it
        // doesn't really matter because the key will mismatch and the execution will continue but if the key is
        // stored externally and the allocated memory is freed it may crash.
        if (HASHTABLE_BUCKET_KEY_VALUE_HAS_FLAG(found_bucket_key_value->flags, HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE)) {
            found_bucket_key = (volatile hashtable_key_data_t*)&found_bucket_key_value->inline_key.data;
            found_bucket_key_max_compare_size = HASHTABLE_INLINE_KEY_MAX_SIZE;
        } else {
            // TODO: The keys must be stored in an append only memory structure to avoid locking, it's mandatory
            //       to avoid crashes!
            found_bucket_key = found_bucket_key_value->external_key.data;
            found_bucket_key_max_compare_size = found_bucket_key_value->external_key.size;
        }

        if (
                HASHTABLE_BUCKET_KEY_VALUE_IS_EMPTY(found_bucket_key_value->flags)
                ||
                HASHTABLE_BUCKET_KEY_VALUE_HAS_FLAG(
                        found_bucket_key_value->flags,
                        HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED)) {
            break;
        }

        if (strncmp(key, (const char *)found_bucket_key, MIN(found_bucket_key_max_compare_size, key_size)) != 0) {
            continue;
        }

        *found_index = index;
        *found_key_value = found_bucket_key_value;
        found = true;
        break;
    }
    HASHTABLE_MEMORY_FENCE_STORE();

    return found;
}

bool hashtable_get(
        hashtable_t* hashtable,
        hashtable_key_data_t* key,
        hashtable_key_size_t key_size,
        hashtable_value_data_t** data) {
    hashtable_bucket_hash_t hash;
    hashtable_bucket_index_t bucket_index;
    volatile hashtable_bucket_key_value_t* bucket_key_value;

    *data = NULL;

    hash = hashtable_calculate_hash(key, key_size);

    volatile hashtable_data_t* hashtable_data_list[] = {
            hashtable->ht_current,
            hashtable->ht_old
    };
    uint8_t hashtable_data_list_size = 2;

    for (
            uint8_t hashtable_data_index = 0;
            hashtable_data_index < hashtable_data_list_size;
            hashtable_data_index++) {
        volatile hashtable_data_t* hashtable_data = hashtable_data_list[hashtable_data_index];

        if (hashtable_data == NULL) {
            continue;
        }

        if (hashtable_search_key(hashtable_data, key, key_size, hash, &bucket_index, &bucket_key_value) == false) {
            continue;
        }

        (*data)->void_data = bucket_key_value->data.void_data;

        HASHTABLE_MEMORY_FENCE_LOAD();
        if (
                hashtable_data->hashes[bucket_index] == hash
                ||
                HASHTABLE_BUCKET_KEY_VALUE_HAS_FLAG(
                        bucket_key_value->flags, HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED)
                ||
                HASHTABLE_BUCKET_KEY_VALUE_IS_EMPTY(
                        bucket_key_value->flags)
                ) {
            *data = NULL;
        }
        HASHTABLE_MEMORY_FENCE_STORE();

        break;
    }

    return *data == NULL ? false : true;
}
