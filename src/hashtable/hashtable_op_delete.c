#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>

#include "memory_fences.h"
#include "log.h"
#include "exttypes.h"
#include "spinlock.h"
#include "xalloc.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_index.h"
#include "hashtable/hashtable_op_delete.h"
#include "hashtable/hashtable_support_hash.h"
#include "hashtable/hashtable_support_op.h"

bool hashtable_op_delete(
        hashtable_t* hashtable,
        hashtable_key_data_t* key,
        hashtable_key_size_t key_size) {
    hashtable_hash_t hash;
    hashtable_key_value_flags_t key_value_flags = 0;
    hashtable_chunk_index_t chunk_index = 0;
    hashtable_chunk_slot_index_t chunk_slot_index = 0;
    hashtable_half_hashes_chunk_volatile_t* half_hashes_chunk;
    hashtable_key_value_volatile_t* key_value;
    bool deleted = false;

    // TODO: the deletion algorithm needs to be updated to compact the keys stored in further away slots relying on the
    //       distance.
    //       Once a slot has been empty-ed, it has to use the AVX2/AVX instructions if available, if not a simple loop,
    //       to search if chunks ahead (within the overflowed_chunks_counter margin) contain keys that can be moved
    //       back to fill the slot.
    //       At the end it has to update the original over overflowed_chunks_counter to restrict the search range if
    //       there was any compaction.

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

        half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index];

        // The get operation is not using locks so the memory fences are needed as well
        spinlock_lock(&half_hashes_chunk->write_lock, true);

        half_hashes_chunk->metadata.is_full = 0;

        half_hashes_chunk->half_hashes[chunk_slot_index].slot_id = 0;

        HASHTABLE_MEMORY_FENCE_STORE();

        key_value_flags = key_value->flags;
        key_value->flags = HASHTABLE_KEY_VALUE_FLAG_DELETED;

        HASHTABLE_MEMORY_FENCE_STORE();

        if (!HASHTABLE_KEY_VALUE_HAS_FLAG(key_value_flags, HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE)) {
            // Even if we have memory fences here, hashtable_op_get may read from the memory that it's going to be
            // de-allocated.
            // Even if it never happened so far even under extremely high concurrency (tested up to 64 logical core with
            // 2048 threads on an AMD EPYC 7502P) it can potentially happen.
            // The append only store will solve this problem once it will be implemented, for the version 0.1 this
            // potentially crash-causing implementation will do well-enough.
            xalloc_free(key_value->external_key.data);
            key_value->external_key.size = 0;
        }

        spinlock_unlock(&half_hashes_chunk->write_lock);

        deleted = true;
    }

    LOG_DI("deleted = %s", deleted ? "YES" : "NO");
    LOG_DI("chunk_index = 0x%016x", chunk_index);
    LOG_DI("chunk_slot_index = 0x%016x", chunk_slot_index);

    return deleted;
}
