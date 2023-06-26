/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <assert.h>
#include <libgen.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_op_iter.h"
#include "data_structures/hashtable/mcmp/hashtable_op_get_key.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "memory_allocator/ffma.h"
#include "log/log.h"
#include "config.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/channel/storage_buffered_channel.h"
#include "storage/storage.h"
#include "storage/storage_buffered.h"
#include "storage/db/storage_db.h"

#include "storage_db_value_type_stream.h"

#define TAG "storage_db_value_type_stream"

#define STORAGE_DB_VALUETYPE_STREAM_KEY_TEMPLATE "%.*s-%lu-%lu"

struct storage_db_value_type_stream_key_value_pairs {
    char *key;
    size_t key_length;
    storage_db_chunk_sequence_t *value_chunk_sequence;
};
typedef struct storage_db_value_type_stream_key_value_pairs storage_db_value_type_stream_key_value_pairs_t;

// Entry information
struct storage_db_value_type_stream_metadata_entry_info {
    uint64_t timestamp;
    uint64_t index;
};
typedef struct storage_db_value_type_stream_metadata_entry_info storage_db_value_type_stream_metadata_entry_info_t;

// Metadata headers containing the count of the entries and the top entry, to figure out if the timestamp can be updated
// of if the entry has the same timestamp and the index needs to be incremented
struct storage_db_value_type_stream_metadata_header {
    uint64_t count;
    storage_db_value_type_stream_metadata_entry_info_t first_entry_info;
    storage_db_value_type_stream_metadata_entry_info_t last_entry_info;
};
typedef struct storage_db_value_type_stream_metadata_header storage_db_value_type_stream_metadata_header_t;

// Index entry for the timestamp minutes index
struct storage_db_value_type_stream_metadata_index_entry {
    uint32_t timestamp_minutes;
    uint32_t segments_counter;
};
typedef struct storage_db_value_type_stream_metadata_index_entry
        storage_db_value_type_stream_metadata_index_entry_t;

// The timestamp minutes index uses a simple list of timestamps in minutes format and a segments counter to indicate
// how many segments there are for that specific minute of data
struct storage_db_value_type_stream_metadata_index {
    uint64_t start;
    uint64_t end;
    uint64_t entries_count;
    storage_db_value_type_stream_metadata_index_entry_t entries[];
};
typedef struct storage_db_value_type_stream_metadata_index
    storage_db_value_type_stream_metadata_index_t;

struct storage_db_value_type_stream_metadata {
    storage_db_value_type_stream_metadata_header_t header;
    storage_db_value_type_stream_metadata_index_t index;
};
typedef struct storage_db_value_type_stream_metadata storage_db_value_type_stream_metadata_t;

size_t storage_db_value_type_stream_calculate_metadata_size(
        uint32_t entries_count) {
    return sizeof(storage_db_value_type_stream_metadata_t) +
           sizeof(storage_db_value_type_stream_metadata_index_entry_t) * entries_count;
}

void storage_db_value_type_stream_build_key(
        char *stream_name,
        size_t stream_name_length,
        storage_db_value_type_stream_metadata_entry_info_t *entry_info,
        char **key,
        size_t *key_length) {

    *key_length = snprintf(
            NULL,
            0,
            STORAGE_DB_VALUETYPE_STREAM_KEY_TEMPLATE,
            (int)stream_name_length,
            stream_name,
            entry_info->timestamp,
            entry_info->index) + 1;

    *key = ffma_mem_alloc(*key_length);

    snprintf(
            *key,
            *key_length,
            STORAGE_DB_VALUETYPE_STREAM_KEY_TEMPLATE,
            (int)stream_name_length,
            stream_name,
            entry_info->timestamp,
            entry_info->index);
}

bool storage_db_value_type_stream_add(
        storage_db_t *storage_db,
        storage_db_database_number_t database_number,
        storage_db_expiry_time_ms_t expiry_time_ms,
        char *stream_name,
        size_t stream_name_length,
        bool entry_info_auto,
        uint64_t entry_info_timestamp,
        bool entry_info_index_auto,
        uint64_t entry_info_index,
        storage_db_value_type_stream_key_value_pairs_t *key_value_pairs,
        size_t key_value_pairs_count) {
    char *data = NULL;
    char *entry_key = NULL;
    size_t entry_key_length = 0;
    uint32_t timestamp_minutes;
    uint32_t timestamp_minutes_shard_index;
    bool return_res = false;
    bool abort_rmw = true;
    bool release_transaction = true;
    bool add_new_timestamp_minutes_index_entry = false;
    bool allocated_new_buffer = false;
    bool allocated_new_metadata = false;
    transaction_t transaction = { 0 };
    storage_db_op_rmw_status_t rmw_status_entry_metadata = { 0 };
    storage_db_op_rmw_status_t rmw_status_entry_timestamp_minute_data = {0 };
    storage_db_entry_index_t *current_entry_index_metadata = NULL;
    storage_db_entry_index_t *new_entry_index_metadata = NULL;
    storage_db_entry_index_t *current_entry_index_timestamp_minute_data = NULL;
    storage_db_entry_index_t *new_entry_index_timestamp_minute_data = NULL;
    storage_db_chunk_sequence_t new_chunk_sequence = { 0 };
    storage_db_chunk_info_t *chunk_info = NULL;
    storage_db_value_type_stream_metadata_t *metadata_original = NULL;
    size_t metadata_original_size;
    storage_db_value_type_stream_metadata_t *metadata_new = NULL;
    size_t metadata_new_size;
    storage_db_value_type_stream_metadata_entry_info_t entry_info = { 0 };

    transaction_acquire(&transaction);

    // Start the RMW transaction on the metadata entry
    if (unlikely(!storage_db_op_rmw_begin(
            storage_db,
            &transaction,
            database_number,
            stream_name,
            stream_name_length,
            &rmw_status_entry_metadata,
            &current_entry_index_metadata))) {
        // TODO: return the reason for the failure
        goto end;
    }

    // Check if the entry exists
    if (likely(current_entry_index_metadata)) {
        // Check if the entry is a stream
        if (unlikely(current_entry_index_metadata->value_type != STORAGE_DB_ENTRY_INDEX_VALUE_TYPE_STREAM)) {
            // TODO: return the reason for the failure
            goto end;
        }

        chunk_info = storage_db_chunk_sequence_get(
                &current_entry_index_metadata->value,
                0);
        data = storage_db_get_chunk_data(
                storage_db,
                chunk_info,
                &allocated_new_buffer);
        if (unlikely(data == NULL)) {
            // TODO: return the reason for the failure
            goto end;
        }

        metadata_original = (storage_db_value_type_stream_metadata_t*)data;
        metadata_original_size = 
    } else {
        // TODO: do we need both allocated_new_metadata and allocated_new_buffer?
        allocated_new_metadata = true;
        allocated_new_buffer = true;

        // If the entry doesn't exist, allocate an empty stream metadata with space for 1 index
        metadata_original = ffma_mem_alloc_zero(sizeof(storage_db_value_type_stream_metadata_t));
        data = (char*)metadata_original;

    }

    // Define the entry info (partially or entirely if the index is passed as well)
    if (likely(entry_info_auto)) {
        entry_info.timestamp = clock_realtime_coarse_int64_ms();
        entry_info_index_auto = true;
    } else {
        entry_info.timestamp = entry_info_timestamp;
        if (!entry_info_index_auto) {
            entry_info.index = entry_info_index;
        }
    }

    // Check if the entry info is valid
    if (unlikely(!entry_info_auto && !allocated_new_metadata)) {
        // Check if the entry info is valid only if it's not automatically determined
        if (likely(!entry_info_auto)) {
            if (unlikely(entry_info.timestamp < metadata_original->header.last_entry_info.timestamp)) {
                // The timestamp is older than the last entry, abort the operation
                // TODO: return the reason for the failure
                goto end;
            } else if (unlikely(entry_info.timestamp == metadata_original->header.last_entry_info.timestamp)) {
                if (unlikely(entry_info.index < metadata_original->header.last_entry_info.index)) {
                    // The index is older than the last entry, abort the operation
                    // TODO: return the reason for the failure
                    goto end;
                }
            }
        }
    }

    // If the entry info index auto is set to true and the timestamp matches the last entry, fetch the index and
    // increments it otherwise can stay with its current value
    if (unlikely(
            !allocated_new_metadata &&
            entry_info_index_auto &&
            entry_info.timestamp == metadata_original->header.last_entry_info.timestamp)) {
        entry_info.index = metadata_original->header.last_entry_info.index + 1;
    }

    // Calculate the timestamp in minutes and get the index
    timestamp_minutes = entry_info.timestamp / (1000 * 60);
    storage_db_value_type_stream_metadata_index_t *timestamp_minutes_index =
            &metadata_original->index;

    // Figure out if it can use the current segment of the current minute or if a new one has to be added
    if (unlikely(timestamp_minutes_index->count == 0 || (timestamp_minutes_index->count > 0 &&
        timestamp_minutes_index->minutes[timestamp_minutes_index->count - 1].timestamp_minutes != timestamp_minutes))) {
        // Need to add a new entry
        add_new_timestamp_minutes_index_entry = true;
    } else {
        if (unlikely(!storage_db_op_rmw_begin(
                storage_db,
                &transaction,
                database_number,
                stream_name,
                stream_name_length,
                &rmw_status_entry_timestamp_minute_data,
                &current_entry_index_timestamp_minute_data))) {
            // TODO: return the reason for the failure
            goto end;
        }

        // Check if the entry exists
        if (unlikely(current_entry_index_timestamp_minute_data == NULL)) {
            // The entry doesn't exist, which shouldn't really be possible
            // TODO: return the reason for the failure
            goto end;
        }

        // Check if the entry is a stream entry
        if (unlikely(current_entry_index_timestamp_minute_data->value_type != STORAGE_DB_ENTRY_INDEX_VALUE_TYPE_STREAM_DATA)) {
            // The entry is not a stream data, shouldn't really be possible
            // TODO: return the reason for the failure
            goto end;
        }

        // Check if there is room in the entry to append the new data
        if (!storage_db_chunk_sequence_is_count_allowed(
                current_entry_index_timestamp_minute_data->value.count + (key_value_pairs_count * 2))) {
            // The entry is full, need to add a new entry, so abort the current rmw operation
            storage_db_op_rmw_abort(
                    storage_db,
                    &rmw_status_entry_timestamp_minute_data);

            add_new_timestamp_minutes_index_entry = true;
        }
    }

    // Figures out if there is an index for the current minute, if not creates it before creating the new segment
    if (unlikely(add_new_timestamp_minutes_index_entry)) {
        if (unlikely(timestamp_minutes_index->count > 0 &&
            timestamp_minutes_index->minutes[timestamp_minutes_index->count - 1].timestamp_minutes != timestamp_minutes)) {
            // The last entry is not the current minute, so add a new entry

            // Duplicate the memory if it's not newly allocated, taking into account it needs extra space for the new
            // entry
            if (!allocated_new_metadata) {
                // Duplicate the memory
                metadata_new = ffma_mem_alloc_zero(sizeof(storage_db_value_type_stream_metadata_t));
                memcpy(metadata_new, metadata_original, sizeof(storage_db_value_type_stream_metadata_t));
                data = (char*)metadata_new;
            } else {
                // Use the newly allocated memory
                metadata_new = metadata_original;
            }

            //

        }
    }








    // Build the new key for the data
    storage_db_value_type_stream_build_key(
            stream_name,
            stream_name_length,
            &entry_info,
            shard_index,
            &entry_key,
            &entry_key_length);

    // Add the new entry
    if (unlikely(!storage_db_op_rmw_begin(
            storage_db,
            &transaction,
            database_number,
            entry_key,
            entry_key_length,
            &rmw_status_entry_metadata,
            &new_entry_index_timestamp_minute_data))) {
        // Failed to begin the RMW operation on the key
        // TODO: return the reason for the failure
        goto end;
    }

    // Ensure that the entry is not already present
    if (unlikely(new_entry_index_timestamp_minute_data)) {
        // The entry already exists, which shouldn't really be possible
        // TODO: return the reason for the failure
        goto end;
    }

    // For performance reason, the data are stored in multiple chunks where the first contains the number of arguments and
    // their length, and the following chunks contain the arguments themselves.
    // The chunks are the ones written already by the external interface to the disk, they are not re-copied, for
    // performance reasons.

    // Prepare the first chunk data containing the number of key value pairs and their length
    size_t chunk_header_buffer_length = sizeof(uint64_t) + (2 * key_value_pairs_count * sizeof(uint32_t));
    if (unlikely(chunk_header_buffer_length > FFMA_OBJECT_SIZE_MAX)) {
        // The chunk header is too big
        goto end;
    }

    char *chunk_header_buffer = ffma_mem_alloc(chunk_header_buffer_length);

    memcpy(chunk_header_buffer, &key_value_pairs_count, sizeof(uint64_t));
    for (size_t i = 0; i < key_value_pairs_count; i++) {
        memcpy(
                chunk_header_buffer + sizeof(uint64_t) + (i * (2 * sizeof(uint32_t))),
                &key_value_pairs[i].key_length,
                sizeof(uint32_t));

        memcpy(
                chunk_header_buffer + sizeof(uint64_t) + ((i * (2 * sizeof(uint32_t))) + sizeof(uint32_t)),
                &key_value_pairs[i].value_chunk_sequence->size,
                sizeof(uint32_t));
    }

    // Allocate the chunk sequence for the new entry
    if (unlikely(!storage_db_chunk_sequence_allocate(
            storage_db,
            &new_chunk_sequence,
            chunk_header_buffer_length))) {
        // TODO: return the reason for the failure
        goto end;
    }

    // Write the entry metadata
    chunk_info = storage_db_chunk_sequence_get(
            &new_chunk_sequence,
            0);

    if (unlikely(!storage_db_chunk_write(
            storage_db,
            chunk_info,
            0,
            chunk_header_buffer,
            chunk_header_buffer_length))) {
        // TODO: return the reason for the failure
        goto end;
    }

    // Loop of the key value pairs to set
    for (size_t key_value_pairs_index = 0; key_value_pairs_index < key_value_pairs_count; key_value_pairs_index++) {
        storage_db_chunk_sequence_t *key_chunk_sequence = NULL;

        if (storage_db->config->backend_type == STORAGE_DB_BACKEND_TYPE_MEMORY) {
            storage_db_chunk_sequence_t key_chunk_sequence_new = { 0 };
            if (!storage_db_chunk_sequence_allocate(
                    storage_db,
                    &key_chunk_sequence_new,
                    key_value_pairs[key_value_pairs_index].key_length)) {
                // TODO: return the reason for the failure
                goto end;
            }

            // The key is always one single chunk so no need to be smart here
            if (!storage_db_chunk_write(
                    storage_db,
                    storage_db_chunk_sequence_get(
                            &key_chunk_sequence_new,
                            0),
                    0,
                    key_value_pairs[key_value_pairs_index].key,
                    key_value_pairs[key_value_pairs_index].key_length)) {
                storage_db_chunk_sequence_free_chunks(storage_db, &key_chunk_sequence_new);

                // TODO: return the reason for the failure
                goto end;
            }

            key_chunk_sequence = &key_chunk_sequence_new;
        }

        // Append the key
        if (!storage_db_chunk_sequence_transfer(
                storage_db,
                key_chunk_sequence,
                &new_chunk_sequence)) {
            // TODO: return the reason for the failure
            goto end;
        }

        // Append the data
        if (!storage_db_chunk_sequence_transfer(
                storage_db,
                key_value_pairs[key_value_pairs_index].value_chunk_sequence,
                &new_chunk_sequence)) {
            // TODO: return the reason for the failure
            goto end;
        }
    }

    if (unlikely(!storage_db_op_rmw_commit_update(
            storage_db,
            &rmw_status_entry_timestamp_minute_data,
            STORAGE_DB_ENTRY_INDEX_VALUE_TYPE_STREAM_DATA,
            &new_chunk_sequence,
            expiry_time_ms))) {
        // TODO: return the reason for the failure
        goto end;
    }



    // TODO: Update the metadata first & last entry as needed
//    if (allocated_new_metadata) {
//        metadata_new->header.first_entry_info = entry_info;
//    }
//    metadata_new->header.last_entry_info = entry_info;

    abort_rmw = false;
    return_res = true;

end:

    if (allocated_new_buffer) {
        ffma_mem_free(data);
    }

    if (entry_key) {
        ffma_mem_free(entry_key);
    }

    if (unlikely(abort_rmw)) {
        storage_db_op_rmw_abort(storage_db, &rmw_status_entry_metadata);
        storage_db_op_rmw_abort(storage_db, &rmw_status_entry_timestamp_minute_data);
    }

    if (unlikely(release_transaction)) {
        transaction_release(&transaction);
    }

    if (unlikely(new_chunk_sequence.sequence)) {
        storage_db_chunk_sequence_free_chunks(storage_db, &new_chunk_sequence);
    }

    return return_res;
}
