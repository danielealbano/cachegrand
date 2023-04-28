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

bool hashtable_mcmp_op_set(
        hashtable_t *hashtable,
        hashtable_database_number_t database_number,
        hashtable_key_data_t *key,
        hashtable_key_length_t key_length,
        hashtable_value_data_t new_value,
        hashtable_value_data_t *previous_value,
        hashtable_bucket_index_t *out_bucket_index,
        bool *out_should_free_key) {
    bool created_new = true;
    hashtable_hash_t hash;
    hashtable_half_hashes_chunk_volatile_t* half_hashes_chunk = 0;
    hashtable_chunk_index_t chunk_index = 0;
    hashtable_chunk_slot_index_t chunk_slot_index = 0;
    hashtable_key_value_volatile_t* key_value = 0;
    transaction_t transaction = { 0 };

    hash = hashtable_mcmp_support_hash_calculate(database_number, key, key_length);

    LOG_DI("key (%d) = %s", key_length, key);
    LOG_DI("hash = 0x%016x", hash);

    assert(*key != 0);

    transaction_acquire(&transaction);

    // TODO: there is no support for resizing right now but when creating a new item the function must be aware that
    //       it has to be created in the new hashtable and not in the one being looked into
    bool ret = hashtable_mcmp_support_op_search_key_or_create_new(
            hashtable->ht_current,
            database_number,
            key,
            key_length,
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

    // Calculate the bucket index
    *out_bucket_index = chunk_index * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT + chunk_slot_index;

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
        key_value->database_number = database_number;
        key_value->key = key;
        key_value->key_length = key_length;

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
    *out_should_free_key = false;
    if (!created_new) {
        *out_should_free_key = true;
    }

    return true;
}
