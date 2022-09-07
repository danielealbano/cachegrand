/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
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

#include "memory_fences.h"
#include "xalloc.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "log/log.h"

#include "hashtable.h"
#include "hashtable_op_set.h"
#include "hashtable_support_hash.h"
#include "hashtable_support_op.h"
#include "hashtable_thread_counters.h"

bool hashtable_mcmp_op_set(
        hashtable_t *hashtable,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_value_data_t new_value,
        hashtable_value_data_t *previous_value) {
    bool created_new = true;
    bool key_inlined = false;
    hashtable_hash_t hash;
    hashtable_half_hashes_chunk_volatile_t* half_hashes_chunk = 0;
    hashtable_chunk_index_t chunk_index = 0;
    hashtable_chunk_slot_index_t chunk_slot_index = 0;
    hashtable_key_value_volatile_t* key_value = 0;
    transaction_t transaction = { 0 };

    hash = hashtable_mcmp_support_hash_calculate(key, key_size);

    LOG_DI("key (%d) = %s", key_size, key);
    LOG_DI("hash = 0x%016x", hash);

    assert(*key != 0);

    transaction_acquire(&transaction);

    // TODO: there is no support for resizing right now but when creating a new item the function must be aware that
    //       it has to be created in the new hashtable and not in the one being looked into
    bool ret = hashtable_mcmp_support_op_search_key_or_create_new(
            hashtable->ht_current,
            key,
            key_size,
            hash,
            true,
            &transaction,
            &created_new,
            &chunk_index,
            &half_hashes_chunk,
            &chunk_slot_index,
            &key_value);

    LOG_DI("created_new = %s", created_new ? "YES" : "NO");
    LOG_DI("half_hashes_chunk = 0x%016x", half_hashes_chunk);
    LOG_DI("key_value =  0x%016x", key_value);

    if (ret == false) {
        transaction_release(&transaction);
        LOG_DI("key not found or not created, continuing");
        return false;
    }

    assert(key_value < hashtable->ht_current->keys_values + hashtable->ht_current->keys_values_size);

    LOG_DI("key found or created");

    MEMORY_FENCE_LOAD();

    if (!created_new && previous_value != NULL) {
        *previous_value = key_value->data;
    }
    key_value->data = new_value;

    LOG_DI("updating value to 0x%016x", new_value);

    if (created_new) {
        LOG_DI("it is a new key, updating flags and key");

        hashtable_key_value_flags_t flags = 0;

        LOG_DI("copying the key onto the key_value structure");

#if HASHTABLE_FLAG_ALLOW_KEY_INLINE == 1
        // Get the destination pointer for the key
        if (key_size <= HASHTABLE_KEY_INLINE_MAX_LENGTH) {
            hashtable_key_data_t* ht_key;

            key_inlined = true;
            LOG_DI("key can be inline-ed", key_size);

            HASHTABLE_KEY_VALUE_SET_FLAG(flags, HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE);

            ht_key = (hashtable_key_data_t *)&key_value->inline_key.data;
            strncpy((char*)ht_key, key, key_size);

            key_value->inline_key.size = key_size;
        } else {
            LOG_DI("key can't be inline-ed, max length for inlining %d", HASHTABLE_KEY_INLINE_MAX_LENGTH);
#endif
            key_value->external_key.data = key;
            key_value->external_key.size = key_size;
#if HASHTABLE_FLAG_ALLOW_KEY_INLINE == 1
        }
#endif

        // Set the FILLED flag
        HASHTABLE_KEY_VALUE_SET_FLAG(flags, HASHTABLE_KEY_VALUE_FLAG_FILLED);

        MEMORY_FENCE_STORE();

        key_value->flags = flags;

        LOG_DI("key_value->flags = %d", key_value->flags);
    }

    // Will perform the memory fence for us
    transaction_release(&transaction);

    LOG_DI("unlocking half_hashes_chunk 0x%016x", half_hashes_chunk);

    // Validate if the passed key can be freed because unused or because inlined
    if (!created_new || key_inlined) {
        xalloc_free(key);
    }

    // Increment the size counter
    hashtable_mcmp_thread_counters_get_current_thread(hashtable)->size += created_new ? 1 : 0;

    return true;
}
