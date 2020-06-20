#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>

#include "memory_fences.h"
#include "log.h"
#include "exttypes.h"
#include "spinlock.h"

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
    hashtable_chunk_index_t chunk_index = 0;
    hashtable_chunk_slot_index_t chunk_slot_index = 0;
    hashtable_key_value_volatile_t* key_value;
    bool deleted = false;

    hash = hashtable_support_hash_calculate(key, key_size);

    LOG_DI("key (%d) = %s", key_size, key);
    LOG_DI("hash = 0x%016x", hash);

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

        LOG_DI("hashtable_data_index = %u", hashtable_data_index);
        LOG_DI("hashtable_data = 0x%016x", hashtable_data);

        if (hashtable_data_index > 0 && (!hashtable->is_resizing || hashtable_data == NULL)) {
            LOG_DI("not resizing, skipping check on the current hashtable_data");
            continue;
        }

        if (hashtable_support_op_search_key(
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

        LOG_DI("key found, deleting hash and setting flags to deleted");

        // It first sets the half hash to zero and only after it resets the flags to deleted.
        hashtable_data->half_hashes_chunk[chunk_index].half_hashes[chunk_slot_index] = 0;
        HASHTABLE_MEMORY_FENCE_STORE();

        key_value->flags = HASHTABLE_KEY_VALUE_FLAG_DELETED;
        HASHTABLE_MEMORY_FENCE_STORE();

        deleted = true;
    }

    LOG_DI("deleted = %s", deleted ? "YES" : "NO");
    LOG_DI("chunk_index = 0x%016x", chunk_index);
    LOG_DI("chunk_slot_index = 0x%016x", chunk_slot_index);

    return deleted;
}
