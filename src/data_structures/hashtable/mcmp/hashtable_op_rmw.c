/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <assert.h>
#include <numa.h>

#include "misc.h"
#include "memory_fences.h"
#include "xalloc.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "log/log.h"

#include "hashtable.h"
#include "hashtable_op_rmw.h"
#include "hashtable_support_hash.h"
#include "hashtable_support_op.h"

bool hashtable_mcmp_op_rmw_begin(
        hashtable_t *hashtable,
        transaction_t *transaction,
        hashtable_mcmp_op_rmw_status_t *rmw_status,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_value_data_t *current_value) {
    bool created_new = false;
    hashtable_hash_t hash;
    hashtable_chunk_index_t chunk_index = 0;
    hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk = 0;
    hashtable_chunk_slot_index_t chunk_slot_index = 0;
    hashtable_key_value_volatile_t *key_value = 0;

    assert(transaction->transaction_id.id != TRANSACTION_ID_NOT_ACQUIRED);

    hash = hashtable_mcmp_support_hash_calculate(key, key_size);

    assert(*key != 0);

    // TODO: there is no support for resizing right now but when creating a new item the function must be aware that
    //       it has to be created in the new hashtable and not in the one being looked into
    bool ret = hashtable_mcmp_support_op_search_key_or_create_new(
            hashtable->ht_current,
            key,
            key_size,
            hash,
            true,
            transaction,
            &created_new,
            &chunk_index,
            &half_hashes_chunk,
            &chunk_slot_index,
            &key_value);

    if (ret == false) {
        return false;
    }

    assert(key_value < hashtable->ht_current->keys_values + hashtable->ht_current->keys_values_size);

    MEMORY_FENCE_LOAD();

    if (!created_new && current_value != NULL) {
        *current_value = key_value->data;
    }

    rmw_status->key = key;
    rmw_status->key_size = key_size;
    rmw_status->hash = hash;
    rmw_status->hashtable = hashtable;
    rmw_status->transaction = transaction;
    rmw_status->half_hashes_chunk = half_hashes_chunk;
    rmw_status->chunk_index = chunk_index;
    rmw_status->chunk_slot_index = chunk_slot_index;
    rmw_status->key_value = key_value;
    rmw_status->created_new = created_new;
    rmw_status->current_value = created_new ? 0 : key_value->data;

    return true;
}

void hashtable_mcmp_op_rmw_commit_update(
        hashtable_mcmp_op_rmw_status_t *rmw_status,
        hashtable_value_data_t new_value) {
    bool key_inlined = false;

    rmw_status->key_value->data = new_value;

    if (rmw_status->created_new) {
        hashtable_key_value_flags_t flags = 0;

#if HASHTABLE_FLAG_ALLOW_KEY_INLINE == 1
        // Get the destination pointer for the key
        if (key_size <= HASHTABLE_KEY_INLINE_MAX_LENGTH) {
            hashtable_key_data_t* ht_key;

            key_inlined = true;

            HASHTABLE_KEY_VALUE_SET_FLAG(flags, HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE);

            ht_key = (hashtable_key_data_t *)&key_value->inline_key.data;
            strncpy((char*)ht_key, key, key_size);

            key_value->inline_key.size = key_size;
        } else {
#endif
        rmw_status->key_value->external_key.data = rmw_status->key;
        rmw_status->key_value->external_key.size = rmw_status->key_size;
#if HASHTABLE_FLAG_ALLOW_KEY_INLINE == 1
        }
#endif

        // Set the FILLED flag
        HASHTABLE_KEY_VALUE_SET_FLAG(flags, HASHTABLE_KEY_VALUE_FLAG_FILLED);

        MEMORY_FENCE_STORE();

        rmw_status->key_value->flags = flags;
    }

    // Validate if the passed key can be freed because unused or because inlined
    if (!rmw_status->created_new || key_inlined) {
        xalloc_free(rmw_status->key);
    }
}

void hashtable_mcmp_op_rmw_commit_delete(
        hashtable_mcmp_op_rmw_status_t *rmw_status) {
    rmw_status->half_hashes_chunk->metadata.slots_occupied--;
    assert(rmw_status->half_hashes_chunk->metadata.slots_occupied <= HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);

    rmw_status->half_hashes_chunk->metadata.is_full = 0;
    rmw_status->half_hashes_chunk->half_hashes[rmw_status->chunk_slot_index].slot_id = 0;
    MEMORY_FENCE_STORE();

    if (likely(!rmw_status->created_new)) {
#if HASHTABLE_FLAG_ALLOW_KEY_INLINE == 1
        hashtable_key_value_flags_t key_value_flags = rmw_status->key_value->flags;
#endif
        rmw_status->key_value->flags = HASHTABLE_KEY_VALUE_FLAG_DELETED;

        MEMORY_FENCE_STORE();

#if HASHTABLE_FLAG_ALLOW_KEY_INLINE == 1
        if (!HASHTABLE_KEY_VALUE_HAS_FLAG(key_value_flags, HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE)) {
#endif
        xalloc_free((hashtable_key_data_t*)rmw_status->key_value->external_key.data);
        rmw_status->key_value->external_key.data = NULL;
        rmw_status->key_value->external_key.size = 0;
#if HASHTABLE_FLAG_ALLOW_KEY_INLINE == 1
        }
#endif
    }
}

void hashtable_mcmp_op_rmw_abort(
        hashtable_mcmp_op_rmw_status_t *rmw_status) {
    if (rmw_status->created_new) {
        rmw_status->half_hashes_chunk->metadata.slots_occupied--;
        assert(rmw_status->half_hashes_chunk->metadata.slots_occupied <= HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);
        rmw_status->half_hashes_chunk->metadata.is_full = 0;
        rmw_status->half_hashes_chunk->half_hashes[rmw_status->chunk_slot_index].slot_id = 0;
    }
}
