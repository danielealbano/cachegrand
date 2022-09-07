/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <numa.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "xalloc.h"
#include "log/log.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"

#include "hashtable.h"

bool hashtable_mcmp_op_get_key(
        hashtable_t *hashtable,
        hashtable_bucket_index_t bucket_index,
        hashtable_key_data_t **key,
        hashtable_key_size_t *key_size) {
    char *source_key = NULL;
    size_t source_key_size = 0;
    hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(bucket_index);
    hashtable_chunk_slot_index_t chunk_slot_index = HASHTABLE_TO_CHUNK_SLOT_INDEX(bucket_index);

    MEMORY_FENCE_LOAD();

    hashtable_data_volatile_t* hashtable_data = hashtable->ht_current;
    hashtable_slot_id_volatile_t slot_id =
            hashtable_data->half_hashes_chunk[chunk_index].half_hashes[chunk_slot_index].slot_id;
    hashtable_key_value_volatile_t *key_value = &hashtable_data->keys_values[bucket_index];

    if (
            unlikely(HASHTABLE_KEY_VALUE_IS_EMPTY(key_value->flags)) ||
            unlikely(HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags, HASHTABLE_KEY_VALUE_FLAG_DELETED))) {
        return false;
    }

#if HASHTABLE_FLAG_ALLOW_KEY_INLINE==1
    if (HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags, HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE)) {
        source_key = key_value->inline_key.data;
        source_key_size = key_value->inline_key.size;
    } else {
#endif
        source_key = key_value->external_key.data;
        source_key_size = key_value->external_key.size;
#if HASHTABLE_FLAG_ALLOW_KEY_INLINE==1
    }
#endif

    assert(source_key != NULL);
    assert(source_key_size > 0);

    *key = xalloc_alloc(source_key_size);
    memcpy(*key, source_key, source_key_size);
    *key_size = source_key_size;

    // Validate that the hash and the key length in the bucket haven't changed or that the bucket has been deleted in
    // the meantime
    MEMORY_FENCE_LOAD();

    bool key_deleted_or_different = false;
    if (unlikely(slot_id != hashtable->ht_current->half_hashes_chunk[chunk_index].half_hashes[chunk_slot_index].slot_id)) {
        key_deleted_or_different = true;
    }

    if (unlikely(!key_deleted_or_different &&
        (HASHTABLE_KEY_VALUE_IS_EMPTY(key_value->flags) ||
         HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags, HASHTABLE_KEY_VALUE_FLAG_DELETED)))) {
        key_deleted_or_different = true;
    }

#if HASHTABLE_FLAG_ALLOW_KEY_INLINE == 1
    if (!HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags, HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE)) {
#endif
    if (unlikely(!key_deleted_or_different && key_value->external_key.size != source_key_size)) {
        key_deleted_or_different = true;
    }
#if HASHTABLE_FLAG_ALLOW_KEY_INLINE == 1
    } else {
        if (unlikely(!key_deleted_or_different && key_value->inline_key.size != source_key_size)) {
            return NULL;
        }
    }
#endif

    if (unlikely(key_deleted_or_different)) {
        ffma_mem_free(key);
        *key = NULL;
        *key_size = 0;
    }

    return *key != NULL;
}
