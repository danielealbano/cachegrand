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
#include <assert.h>
#include <numa.h>

#include "misc.h"
#include "memory_fences.h"
#include "exttypes.h"
#include "spinlock.h"
#include "log/log.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "slab_allocator.h"

#include "hashtable.h"
#include "hashtable_op_rmw.h"
#include "hashtable_support_hash.h"
#include "hashtable_support_op.h"

bool hashtable_mcmp_op_rmw_begin(
        hashtable_t *hashtable,
        hashtable_mcmp_op_rmw_transaction_t *rmw_transaction,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_value_data_t *current_value) {
    bool created_new = true;
    hashtable_hash_t hash;
    hashtable_chunk_index_t chunk_index = 0;
    hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk = 0;
    hashtable_chunk_slot_index_t chunk_slot_index = 0;
    hashtable_key_value_volatile_t *key_value = 0;

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

    rmw_transaction->key = key;
    rmw_transaction->key_size = key_size;
    rmw_transaction->hash = hash;
    rmw_transaction->half_hashes_chunk = half_hashes_chunk;
    rmw_transaction->chunk_index = chunk_index;
    rmw_transaction->chunk_slot_index = chunk_slot_index;
    rmw_transaction->key_value = key_value;
    rmw_transaction->created_new = created_new;
    rmw_transaction->previous_entry_index = created_new ? 0 : key_value->data;

    return true;
}

void hashtable_mcmp_op_rmw_commit(
        hashtable_mcmp_op_rmw_transaction_t *rmw_transaction,
        hashtable_value_data_t new_value) {
    bool key_inlined = false;

    rmw_transaction->key_value->data = new_value;

    if (rmw_transaction->created_new) {
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
        rmw_transaction->key_value->external_key.data = rmw_transaction->key;
        rmw_transaction->key_value->external_key.size = rmw_transaction->key_size;
#if HASHTABLE_FLAG_ALLOW_KEY_INLINE == 1
        }
#endif

        // Set the FILLED flag
        HASHTABLE_KEY_VALUE_SET_FLAG(flags, HASHTABLE_KEY_VALUE_FLAG_FILLED);

        MEMORY_FENCE_STORE();

        rmw_transaction->key_value->flags = flags;
    }

    // Will perform the memory fence for us
    spinlock_unlock(&rmw_transaction->half_hashes_chunk->write_lock);

    // Validate if the passed key can be freed because unused or because inlined
    if (!rmw_transaction->created_new || key_inlined) {
        slab_allocator_mem_free(rmw_transaction->key);
    }
}

void hashtable_mcmp_op_rmw_abort(
        hashtable_mcmp_op_rmw_transaction_t *rmw_transaction) {
    if (rmw_transaction->created_new) {
        rmw_transaction->half_hashes_chunk->half_hashes[rmw_transaction->chunk_slot_index].slot_id = 0;
    }

    // Will perform the memory fence for us
    spinlock_unlock(&rmw_transaction->half_hashes_chunk->write_lock);
}
