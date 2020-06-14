#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>

#include "memory_fences.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_index.h"
#include "hashtable/hashtable_op_delete.h"
#include "hashtable/hashtable_support_hash.h"
#include "hashtable/hashtable_support_op.h"

// TODO: support the new data structure
bool hashtable_op_delete(
        hashtable_t* hashtable,
        hashtable_key_data_t* key,
        hashtable_key_size_t key_size) {
    hashtable_hash_t hash;
    hashtable_bucket_index_t bucket_index;
    volatile hashtable_key_value_t* bucket_key_value;
    bool deleted = false;
/*
    hash = hashtable_support_hash_calculate(key, key_size);

    volatile hashtable_data_t* hashtable_data_list[] = {
            hashtable->ht_current,
            hashtable->ht_old
    };
    uint8_t hashtable_data_list_size = 2;

    for (
            uint8_t hashtable_data_index = 0;
            hashtable_data_index < hashtable_data_list_size && deleted == false;
            hashtable_data_index++) {
        volatile hashtable_data_t *hashtable_data = hashtable_data_list[hashtable_data_index];

        if (hashtable_data == NULL) {
            continue;
        }

        if (hashtable_support_op_search_key(
                hashtable_data,
                key,
                key_size,
                hash,
                &bucket_key_value) == false) {
            continue;
        }

        hashtable_data->hashes[bucket_index] = 0;
        bucket_key_value->flags = HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED;
        HASHTABLE_MEMORY_FENCE_STORE();

        deleted = true;
    }
*/
    return deleted;
}
