/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
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

#include "misc.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "transaction.h"
#include "log/log.h"

#include "hashtable.h"
#include "hashtable_op_get.h"
#include "hashtable_support_hash.h"
#include "hashtable_support_op.h"

bool hashtable_mcmp_op_get(
        hashtable_t *hashtable,
        hashtable_database_number_t database_number,
        transaction_t *transaction,
        hashtable_key_data_t *key,
        hashtable_key_length_t key_length,
        hashtable_value_data_t *data) {
    hashtable_hash_t hash;
    hashtable_chunk_index_t chunk_index = 0;
    hashtable_chunk_slot_index_t chunk_slot_index = 0;
    hashtable_key_value_volatile_t* key_value = 0;

    bool data_found = false;
    *data = 0;

    hash = hashtable_mcmp_support_hash_calculate(database_number, key, key_length);

    LOG_DI("key (%d) = %s", key_length, key);
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
        MEMORY_FENCE_LOAD();

        hashtable_data_volatile_t* hashtable_data = hashtable_data_list[hashtable_data_index];

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
                transaction,
                &chunk_index,
                &chunk_slot_index,
                &key_value) == false) {
            LOG_DI("key not found, continuing");
            continue;
        }

        LOG_DI("key found, fetching value");

        MEMORY_FENCE_LOAD();

        *data = key_value->data;

        data_found = true;

        break;
    }

    LOG_DI("data_found = %s", data_found ? "YES" : "NO");
    LOG_DI("data = 0x%016x", data);

    return data_found;
}

bool hashtable_mcmp_op_get_by_index(
        hashtable_t *hashtable,
        hashtable_database_number_t database_number,
        transaction_t *transaction,
        hashtable_bucket_index_t bucket_index,
        hashtable_value_data_t *current_value) {
    hashtable_half_hashes_chunk_volatile_t* half_hashes_chunk;
    hashtable_chunk_index_t chunk_index;
    hashtable_chunk_slot_index_t chunk_slot_index;
    hashtable_key_value_volatile_t* key_value;

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

        if (unlikely(!transaction_lock_for_read(transaction, &half_hashes_chunk->lock))) {
            return false;
        }

        key_value = &hashtable_data->keys_values[bucket_index];

        if (unlikely(key_value->database_number != database_number)) {
            return false;
        }

        if (current_value != NULL) {
            *current_value = key_value->data;
        }

        return true;
    }

    return false;
}

bool hashtable_mcmp_op_get_by_index_all_databases(
        hashtable_t *hashtable,
        transaction_t *transaction,
        hashtable_bucket_index_t bucket_index,
        hashtable_value_data_t *current_value) {
    hashtable_half_hashes_chunk_volatile_t* half_hashes_chunk;
    hashtable_chunk_index_t chunk_index;
    hashtable_chunk_slot_index_t chunk_slot_index;
    hashtable_key_value_volatile_t* key_value;

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

        if (unlikely(!transaction_lock_for_read(transaction, &half_hashes_chunk->lock))) {
            return false;
        }

        key_value = &hashtable_data->keys_values[bucket_index];

        if (current_value != NULL) {
            *current_value = key_value->data;
        }

        return true;
    }

    return false;
}
