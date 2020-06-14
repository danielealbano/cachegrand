#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>

#include "memory_fences.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_op_get.h"
#include "hashtable/hashtable_support_index.h"
#include "hashtable/hashtable_support_hash.h"
#include "hashtable/hashtable_support_op.h"

bool hashtable_op_get(
        hashtable_t *hashtable,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_value_data_t *data) {
    hashtable_hash_t hash;
    volatile hashtable_key_value_t* key_value = 0;

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
        HASHTABLE_MEMORY_FENCE_LOAD();

        volatile hashtable_data_t* hashtable_data = hashtable_data_list[hashtable_data_index];

        if (hashtable_data_index > 0 && (!hashtable->is_resizing || hashtable_data == NULL)) {
            continue;
        }

        if (hashtable_support_op_search_key(
                hashtable_data,
                key,
                key_size,
                hash,
                &key_value) == false) {
            continue;
        }

        HASHTABLE_MEMORY_FENCE_LOAD();

        *data = key_value->data;
        data_found = true;

        break;
    }

    return data_found;
}
