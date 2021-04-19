#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>

#include "memory_fences.h"
#include "exttypes.h"
#include "spinlock.h"
#include "log/log.h"

#include "hashtable.h"
#include "hashtable_op_get.h"
#include "hashtable_support_hash.h"
#include "hashtable_support_op.h"

bool hashtable_mcmp_op_get(
        hashtable_t *hashtable,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_value_data_t *data) {
    hashtable_hash_t hash;
    hashtable_chunk_index_t chunk_index = 0;
    hashtable_chunk_slot_index_t chunk_slot_index = 0;
    hashtable_key_value_volatile_t* key_value = 0;

    bool data_found = false;
    *data = 0;

    hash = hashtable_mcmp_support_hash_calculate(key, key_size);

    LOG_DI("key (%d) = %s", key_size, key);
    LOG_DI("hash = 0x%016x", hash);

    hashtable_data_volatile_t* hashtable_data_list[] = {
            hashtable->ht_current,
            hashtable->ht_old
    };
    uint8_t hashtable_data_list_size = 2;

    for (
            uint8_t hashtable_data_index = 0;
            hashtable_data_index < hashtable_data_list_size;
            hashtable_data_index++) {
        HASHTABLE_MEMORY_FENCE_LOAD();

        hashtable_data_volatile_t* hashtable_data = hashtable_data_list[hashtable_data_index];

        LOG_DI("hashtable_data_index = %u", hashtable_data_index);
        LOG_DI("hashtable_data = 0x%016x", hashtable_data);

        if (hashtable_data_index > 0 && (!hashtable->is_resizing || hashtable_data == NULL)) {
            LOG_DI("not resizing, skipping check on the current hashtable_data");
            continue;
        }

        if (hashtable_mcmp_support_op_search_key(
                hashtable_data,
                key,
                key_size,
                hash,
                &chunk_index,
                &chunk_slot_index,
                &key_value) == false) {
            LOG_DI("key not found, continuing");
            continue;
        }

        LOG_DI("key found, fetching value");

        HASHTABLE_MEMORY_FENCE_LOAD();

        *data = key_value->data;

        // Note for the future
        // ---
        // At this point, the code can end-up returning a value of a key that has been deleted.
        // While this is a non problem, this is something that would happen anyway in a context of a network platform
        // that receives un-syncronized requests from the caller and that if it's necessary the caller has to implement
        // the required sync mechanism to ensure it's not going to issue get/delete commands in the wrong order, if it
        // will be necessary to avoid returning deleted data a check on flags can be introduced in this point

        data_found = true;

        break;
    }

    LOG_DI("data_found = %s", data_found ? "YES" : "NO");
    LOG_DI("data = 0x%016x", data);

    return data_found;
}
