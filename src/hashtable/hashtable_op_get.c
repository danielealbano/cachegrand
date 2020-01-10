#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>

#include "hashtable.h"
#include "hashtable_op_get.h"
#include "hashtable_support_index.h"
#include "hashtable_support_hash.h"
#include "hashtable_support_op.h"

bool hashtable_get(
        hashtable_t* hashtable,
        hashtable_key_data_t* key,
        hashtable_key_size_t key_size,
        hashtable_value_data_t* data) {
    hashtable_bucket_hash_t hash;
    hashtable_bucket_index_t bucket_index;
    volatile hashtable_bucket_key_value_t* bucket_key_value;

    bool data_found = false;
    *data = 0;

    hash = hashtable_support_hash_calculate(key, key_size);

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

        if (hashtable_support_op_search_key(hashtable_data, key, key_size, hash, &bucket_index, &bucket_key_value) == false) {
            continue;
        }

        *data = bucket_key_value->data;
        data_found = true;

        HASHTABLE_MEMORY_FENCE_LOAD();
        if (
                hashtable_data->hashes[bucket_index] != hash
                ||
                HASHTABLE_BUCKET_KEY_VALUE_HAS_FLAG(
                        bucket_key_value->flags, HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED)
                ||
                HASHTABLE_BUCKET_KEY_VALUE_IS_EMPTY(
                        bucket_key_value->flags)
                ) {
            *data = 0;
            data_found = false;
        }
        HASHTABLE_MEMORY_FENCE_STORE();

        break;
    }

    return data_found;
}
