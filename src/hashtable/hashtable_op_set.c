#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>

#include "xalloc.h"

#include "hashtable.h"
#include "hashtable_op_set.h"
#include "hashtable_support_index.h"
#include "hashtable_support_hash.h"
#include "hashtable_support_op.h"

bool hashtable_op_set(
        hashtable_t *hashtable,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_value_data_t value) {
    bool created_new;
    hashtable_search_key_or_create_new_ret_t ret;
    hashtable_bucket_index_t bucket_index;
    hashtable_bucket_hash_t hash;
    hashtable_key_data_t* ht_key;
    bool resized = false;
    volatile hashtable_bucket_key_value_t* bucket_key_value;

    hash = hashtable_support_hash_calculate(key, key_size);

    do {
        ret = hashtable_support_op_search_key_or_create_new(
                hashtable->ht_current,
                key,
                key_size,
                hash,
                &created_new,
                &bucket_index,
                &bucket_key_value);

        if (ret == HASHTABLE_SEARCH_KEY_OR_CREATE_NEW_RET_NO_FREE) {
            if (!resized && hashtable->config->can_auto_resize) {
                // TODO: implement (auto)resize
                // hashtable_scale_up_start(hashtable);
                resized = true;
                continue;
            }

            break;
        }
    }
    while(ret == HASHTABLE_SEARCH_KEY_OR_CREATE_NEW_RET_NO_FREE);

    if (ret == HASHTABLE_SEARCH_KEY_OR_CREATE_NEW_RET_EMPTY_OR_DELETED ||
        ret == HASHTABLE_SEARCH_KEY_OR_CREATE_NEW_RET_NO_FREE) {
        return false;
    }

    HASHTABLE_MEMORY_FENCE_LOAD();

    bucket_key_value->data = value;

    if (!created_new) {
        HASHTABLE_MEMORY_FENCE_LOAD();

        if (HASHTABLE_BUCKET_KEY_VALUE_HAS_FLAG(bucket_key_value->flags, HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED)) {
            return false;
        }
    } else {
        hashtable_bucket_key_value_flags_t flags = 0;

        // Get the destination pointer for the key
        if (key_size <= HASHTABLE_KEY_INLINE_MAX_LENGTH) {
            ht_key = (hashtable_key_data_t *)&bucket_key_value->inline_key.data;
            HASHTABLE_BUCKET_KEY_VALUE_SET_FLAG(flags, HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE);
        } else {
            // TODO: The keys must be stored in an append only memory structure to avoid locking, memory can't be freed
            //       immediately after the bucket is freed because it can be in use and would cause a crash34567
            ht_key = xalloc_alloc(key_size + 1);
            ht_key[key_size] = '\0';

            bucket_key_value->external_key.data = ht_key;
            bucket_key_value->external_key.size = key_size;
        }

        // Copy the key
        strncpy((char*)ht_key, key, key_size);

        // Set the FILLED flag (drops the deleted flag as well)
        HASHTABLE_BUCKET_KEY_VALUE_SET_FLAG(flags, HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED);

        HASHTABLE_MEMORY_FENCE_STORE();

        // Update the flags atomically
        bucket_key_value->flags = flags;
    }

    return true;
}
