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
#include <numa.h>

#include "misc.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "xalloc.h"
#include "log/log.h"
#include "spinlock.h"
#include "transaction.h"

#include "hashtable.h"
#include "hashtable_support_index.h"
#include "hashtable_op_delete.h"
#include "hashtable_support_hash.h"
#include "hashtable_support_op.h"

bool hashtable_mcmp_op_delete(
        hashtable_t* hashtable,
        hashtable_database_number_t database_number,
        hashtable_key_data_t* key,
        hashtable_key_length_t key_length,
        hashtable_value_data_t *current_value) {
    hashtable_hash_t hash;
    hashtable_chunk_index_t chunk_index = 0;
    hashtable_chunk_slot_index_t chunk_slot_index = 0;
    hashtable_half_hashes_chunk_volatile_t* half_hashes_chunk;
    hashtable_key_value_volatile_t* key_value;
    transaction_t transaction = { 0 };
    bool deleted = false;

    // TODO: the deletion algorithm needs to be updated to compact the keys stored in further away slots relying on the
    //       distance.
    //       Once a slot has been empty-ed, it has to use the AVX2/AVX instructions if available, if not a simple loop,
    //       to search if chunks ahead (within the overflowed_chunks_counter margin) contain keys that can be moved
    //       back to fill the slot.
    //       At the end it has to update the original over overflowed_chunks_counter to restrict the search range if
    //       there was any compaction.

    hash = hashtable_mcmp_support_hash_calculate(database_number, key, key_length);

    LOG_DI("key (%d) = %s", key_length, key);
    LOG_DI("hash = 0x%016x", hash);
    transaction_acquire(&transaction);

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

        if (hashtable_mcmp_support_op_search_key(
                hashtable_data,
                database_number,
                key,
                key_length,
                hash,
                &transaction,
                &chunk_index,
                &chunk_slot_index,
                &key_value) == false) {
            LOG_DI("key not found, continuing");
            continue;
        }

        LOG_DI("key found, deleting hash and setting flags to deleted");

        half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index];

        if (half_hashes_chunk->half_hashes[chunk_slot_index].filled == 0) {
            return false;
        }

        if (unlikely(!transaction_upgrade_lock_for_write(&transaction, &half_hashes_chunk->lock))) {
            return false;
        }

        // The hashtable_mcmp_support_op_search_key operation is lockless, it's necessary to set the lock and validate
        // that the hash initially found it's the same to avoid a potential race condition.
        hashtable_hash_quarter_t quarter_hash =
                hashtable_mcmp_support_hash_quarter(hashtable_mcmp_support_hash_half(hash));
        if (likely(half_hashes_chunk->half_hashes[chunk_slot_index].quarter_hash == quarter_hash)) {
            if (current_value != NULL) {
                *current_value = key_value->data;
            }

            half_hashes_chunk->metadata.slots_occupied--;
            assert(half_hashes_chunk->metadata.slots_occupied <= HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);
            half_hashes_chunk->metadata.is_full = 0;
            half_hashes_chunk->half_hashes[chunk_slot_index].slot_id = 0;

            MEMORY_FENCE_STORE();

            key_value->flags = HASHTABLE_KEY_VALUE_FLAG_DELETED;

            MEMORY_FENCE_STORE();

            xalloc_free((hashtable_key_data_t*)key_value->key);
            key_value->database_number = 0;
            key_value->key = NULL;
            key_value->key_length = 0;

            deleted = true;
        }

        transaction_release(&transaction);

        if (likely(deleted)) {
            break;
        }
    }

    LOG_DI("deleted = %s", deleted ? "YES" : "NO");
    LOG_DI("chunk_index = 0x%016x", chunk_index);
    LOG_DI("chunk_slot_index = 0x%016x", chunk_slot_index);

    return deleted;
}

bool hashtable_mcmp_op_delete_by_index(
        hashtable_t* hashtable,
        hashtable_database_number_t database_number,
        hashtable_bucket_index_t bucket_index,
        hashtable_value_data_t *current_value) {
    hashtable_chunk_index_t chunk_index = 0;
    hashtable_chunk_slot_index_t chunk_slot_index = 0;
    hashtable_half_hashes_chunk_volatile_t* half_hashes_chunk;
    hashtable_key_value_volatile_t* key_value;
    transaction_t transaction = { 0 };
    bool deleted = false;

    volatile hashtable_data_t* hashtable_data_list[] = {
            hashtable->ht_current,
            hashtable->ht_old
    };
    uint8_t hashtable_data_list_size = 2;

    for (
            uint8_t hashtable_data_index = 0;
            hashtable_data_index < hashtable_data_list_size;
            hashtable_data_index++) {
        volatile hashtable_data_t *hashtable_data = hashtable_data_list[hashtable_data_index];

        LOG_DI("hashtable_data_index = %u", hashtable_data_index);
        LOG_DI("hashtable_data = 0x%016x", hashtable_data);

        if (hashtable_data_index > 0 && (!hashtable->is_resizing || hashtable_data == NULL)) {
            LOG_DI("not resizing, skipping check on the current hashtable_data");
            continue;
        }

        chunk_index = bucket_index / HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;
        chunk_slot_index = bucket_index % HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;

        half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index];

        if (half_hashes_chunk->half_hashes[chunk_slot_index].filled == 0) {
            return false;
        }

        transaction_acquire(&transaction);
        if (unlikely(!transaction_lock_for_write(&transaction, &half_hashes_chunk->lock))) {
            return false;
        }

        key_value = &hashtable_data->keys_values[bucket_index];

        if (unlikely(key_value->database_number != database_number)) {
            transaction_release(&transaction);
            return false;
        }

        if (current_value != NULL) {
            *current_value = key_value->data;
        }

        half_hashes_chunk->metadata.slots_occupied--;
        assert(half_hashes_chunk->metadata.slots_occupied <= HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);
        half_hashes_chunk->metadata.is_full = 0;
        half_hashes_chunk->half_hashes[chunk_slot_index].slot_id = 0;

        MEMORY_FENCE_STORE();

        key_value->flags = HASHTABLE_KEY_VALUE_FLAG_DELETED;

        MEMORY_FENCE_STORE();

        xalloc_free((hashtable_key_data_t*)key_value->key);
        key_value->database_number = 0;
        key_value->key = NULL;
        key_value->key_length = 0;

        deleted = true;

        transaction_release(&transaction);

        break;
    }

    LOG_DI("deleted = %s", deleted ? "YES" : "NO");
    LOG_DI("chunk_index = 0x%016x", chunk_index);
    LOG_DI("chunk_slot_index = 0x%016x", chunk_slot_index);

    return deleted;
}

bool hashtable_mcmp_op_delete_by_index_all_databases(
        hashtable_t* hashtable,
        hashtable_bucket_index_t bucket_index,
        hashtable_value_data_t *current_value) {
    hashtable_chunk_index_t chunk_index = 0;
    hashtable_chunk_slot_index_t chunk_slot_index = 0;
    hashtable_half_hashes_chunk_volatile_t* half_hashes_chunk;
    hashtable_key_value_volatile_t* key_value;
    transaction_t transaction = { 0 };
    bool deleted = false;

    volatile hashtable_data_t* hashtable_data_list[] = {
            hashtable->ht_current,
            hashtable->ht_old
    };
    uint8_t hashtable_data_list_size = 2;

    for (
            uint8_t hashtable_data_index = 0;
            hashtable_data_index < hashtable_data_list_size;
            hashtable_data_index++) {
        volatile hashtable_data_t *hashtable_data = hashtable_data_list[hashtable_data_index];

        LOG_DI("hashtable_data_index = %u", hashtable_data_index);
        LOG_DI("hashtable_data = 0x%016x", hashtable_data);

        if (hashtable_data_index > 0 && (!hashtable->is_resizing || hashtable_data == NULL)) {
            LOG_DI("not resizing, skipping check on the current hashtable_data");
            continue;
        }

        chunk_index = bucket_index / HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;
        chunk_slot_index = bucket_index % HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;

        half_hashes_chunk = &hashtable_data->half_hashes_chunk[chunk_index];

        if (half_hashes_chunk->half_hashes[chunk_slot_index].filled == 0) {
            return false;
        }

        transaction_acquire(&transaction);
        if (unlikely(!transaction_lock_for_write(&transaction, &half_hashes_chunk->lock))) {
            return false;
        }

        key_value = &hashtable_data->keys_values[bucket_index];

        if (current_value != NULL) {
            *current_value = key_value->data;
        }

        half_hashes_chunk->metadata.slots_occupied--;
        assert(half_hashes_chunk->metadata.slots_occupied <= HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);
        half_hashes_chunk->metadata.is_full = 0;
        half_hashes_chunk->half_hashes[chunk_slot_index].slot_id = 0;

        MEMORY_FENCE_STORE();

        key_value->flags = HASHTABLE_KEY_VALUE_FLAG_DELETED;

        MEMORY_FENCE_STORE();

        xalloc_free((hashtable_key_data_t*)key_value->key);
        key_value->database_number = 0;
        key_value->key = NULL;
        key_value->key_length = 0;

        deleted = true;

        transaction_release(&transaction);

        break;
    }

    LOG_DI("deleted = %s", deleted ? "YES" : "NO");
    LOG_DI("chunk_index = 0x%016x", chunk_index);
    LOG_DI("chunk_slot_index = 0x%016x", chunk_slot_index);

    return deleted;
}
