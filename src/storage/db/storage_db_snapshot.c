/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
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
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "memory_allocator/ffma.h"
#include "log/log.h"
#include "config.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/storage.h"
#include "storage/db/storage_db.h"
#include "module/redis/snapshot/module_redis_snapshot.h"
#include "module/redis/snapshot/module_redis_snapshot_serialize_primitive.h"

#include "storage_db_snapshot.h"

#define TAG "storage_db_snapshot"

bool storage_db_snapshot_rdb_write_buffer(
        storage_db_t *db,
        uint8_t *buffer,
        size_t buffer_size) {
    // Write the buffer to the snapshot file
    if (!storage_write(
            db->snapshot.storage_channel,
            (char*)buffer,
            buffer_size,
            db->snapshot.offset)) {
        LOG_E(TAG, "Failed to write the snapshot buffer");
        return false;
    }

    db->snapshot.offset += (off_t)buffer_size;

    return true;
}

void storage_db_snapshot_completed(
        storage_db_t *db,
        storage_db_snapshot_status_t status) {
    // Free the path
    ffma_mem_free(db->snapshot.storage_channel->path);

    // Update the snapshot general information
    storage_db_snapshot_update_next_run_time(db);
    db->snapshot.end_time_ms = clock_monotonic_coarse_int64_ms();
    db->snapshot.status = status;

    // Report the result of the snapshot
    LOG_I(
            TAG,
            status == STORAGE_DB_SNAPSHOT_STATUS_COMPLETED
            ? "Snapshot completed in <%lu> ms"
            : "Snapshot failed after <%lu> ms",
            db->snapshot.end_time_ms - db->snapshot.start_time_ms);

    // Sync the data
    db->snapshot.running = false;
    MEMORY_FENCE_STORE();
}

void storage_db_snapshot_failed(
        storage_db_t *db) {
    // Close the snapshot file and delete the file from the disk
    storage_close(db->snapshot.storage_channel);
    unlink(db->snapshot.storage_channel->path);

    // The operation has failed so the storage channel must be closed and the snapshot deleted
    storage_db_snapshot_completed(db, STORAGE_DB_SNAPSHOT_STATUS_FAILED);
}

bool storage_db_snapshot_should_run(
        storage_db_t *db) {
    storage_db_counters_t counters;
    storage_db_config_t *config = db->config;

    // If the snapshot is running skip all the checks
    if (db->snapshot.running) {
        return true;
    }

    // Check if the snapshot feature is enabled
    if (likely(!config->snapshot.enabled)) {
        return false;
    }

    // Check if the configured interval has passed
    uint64_t now = clock_monotonic_coarse_int64_ms();
    uint64_t next_snapshot_time_ms = db->snapshot.next_run_time_ms;
    if (now < next_snapshot_time_ms) {
        return false;
    }

    // Check if there is any configured constraint to run the snapshot
    if (config->snapshot.min_keys_changed > 0 || config->snapshot.min_data_changed > 0) {
        storage_db_counters_sum(db, &counters);

        // Check if the number of keys changed is greater than the configured threshold
        uint64_t keys_changed = counters.keys_changed - db->snapshot.keys_changed_at_start;
        if (config->snapshot.min_keys_changed > 0 && keys_changed >= config->snapshot.min_keys_changed) {
            return true;
        }

        // Check if the number of data changed is greater than the configured threshold
        uint64_t data_changed = counters.data_changed - db->snapshot.data_changed_at_start;
        if (config->snapshot.min_data_changed > 0 && data_changed >= config->snapshot.min_data_changed) {
            return true;
        }
    }

    return true;
}

void storage_db_snapshot_update_next_run_time(
        storage_db_t *db) {
    storage_db_config_t *config = db->config;

    // Set the next run time
    db->snapshot.next_run_time_ms = clock_monotonic_coarse_int64_ms() + config->snapshot.interval_ms;
}

void storage_db_snapshot_wait_for_prepared(
        storage_db_t *db) {
    // Wait for the snapshot to be ready
    do {
        MEMORY_FENCE_LOAD();
    } while (db->snapshot.status != STORAGE_DB_SNAPSHOT_STATUS_IN_PREPARATION);
}

bool storage_db_snapshot_rdb_prepare(
        storage_db_t *db) {
    storage_io_common_fd_t snapshot_fd;
    bool result = true;
    char snapshot_path[PATH_MAX];
    struct stat parent_path_stat;
    storage_db_config_t *config = db->config;

    // Set the storage channel to NULL
    db->snapshot.storage_channel = NULL;

    // Ensure that the parent directory of the path to be used for the snapshot exists and validates that it's readable
    // and writable
    char snapshot_path_tmp[PATH_MAX + 1];
    strcpy(snapshot_path_tmp, config->snapshot.path);
    char* parent_path = dirname(snapshot_path_tmp);
    if(stat(parent_path, &parent_path_stat) == -1) {
        if(ENOENT == errno) {
            LOG_E(TAG, "The folder for the snapshots path <%s> does not exist", parent_path);
        } else {
            LOG_E(TAG, "Unable to check the folder for the snapshots path <%s>", parent_path);
        }
        result = false;
        goto end;
    } else {
        if (!S_ISDIR(parent_path_stat.st_mode)) {
            LOG_E(TAG, "The folder for the snapshots path <%s> is not a folder", parent_path);
            result = false;
            goto end;
        } else if(access(parent_path, R_OK | W_OK) == -1) {
            LOG_E(TAG, "The folder for the snapshots path <%s> is not readable or writable", parent_path);
            result = false;
            goto end;
        }
    }

    // Generate a random path for the snapshot based on the requested path
    strncpy(snapshot_path, config->snapshot.path, PATH_MAX - 1);
    strncat(snapshot_path, ".XXXXXX", PATH_MAX - 1);
    if ((snapshot_fd = mkstemp(snapshot_path)) == -1) {
        LOG_E(TAG, "Unable to generate a temporary path for the snapshot");
        LOG_E_OS_ERROR(TAG);
        result = false;
        goto end;
    }

    // If the snapshot was generated correctly, the file description has to be encapsulated in a storage_channel_t
    // object to safely operate on it using the storage subsystem and the fibers.
    if ((db->snapshot.storage_channel = storage_open_fd(snapshot_fd)) == NULL) {
        LOG_E(TAG, "Unable to open the snapshot file descriptor");
        result = false;
        goto end;
    }

    // Duplicate the path of the storage channel
    db->snapshot.path = ffma_mem_alloc(db->snapshot.storage_channel->path_len + 1);
    strncpy(db->snapshot.path, db->snapshot.storage_channel->path, db->snapshot.storage_channel->path_len);

    // Write the snapshot header
    uint8_t buffer[128] = { 0 };
    size_t buffer_size = sizeof(buffer);
    size_t buffer_offset_out = 0;
    module_redis_snapshot_header_t header = { .version = STORAGE_DB_SNAPSHOT_RDB_VERSION };
    if (module_redis_snapshot_serialize_primitive_encode_header(
            &header,
            buffer,
            buffer_size,
            0,
            &buffer_offset_out) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
        LOG_E(TAG, "Failed to prepare the snapshot header");
        result = false;
        goto end;
    }

    if (!storage_db_snapshot_rdb_write_buffer(db, buffer, buffer_offset_out)) {
        LOG_E(TAG, "Failed to write the snapshot header");
        result = false;
        goto end;
    }

end:
    if (result) {
        storage_db_counters_t counters;
        storage_db_counters_sum(db, &counters);

        // The snapshot has been successfully prepared, so we can set running to true and update the status to
        // IN_PROGRESS
        db->snapshot.keys_changed_at_start = counters.keys_changed;
        db->snapshot.data_changed_at_start = counters.data_changed;
        db->snapshot.start_time_ms = clock_monotonic_coarse_int64_ms();
        db->snapshot.running = true;
        db->snapshot.status = STORAGE_DB_SNAPSHOT_STATUS_IN_PROGRESS;
        MEMORY_FENCE_STORE();
    } else {
        storage_db_snapshot_failed(db);
    }

    return result;
}

bool storage_db_snapshot_rdb_ensure_prepared(
        storage_db_t *db) {
    MEMORY_FENCE_LOAD();
    if (db->snapshot.running) {
        return true;
    }

    // Check if the status is different from IN_PREPARATION, in which case the snapshot is being prepared by another
    // thread so we just need to wait for it to be ready
    MEMORY_FENCE_LOAD();
    if (db->snapshot.status == STORAGE_DB_SNAPSHOT_STATUS_IN_PREPARATION) {
        storage_db_snapshot_wait_for_prepared(db);
        return db->snapshot.status != STORAGE_DB_SNAPSHOT_STATUS_FAILED;
    }

    // Try to set the status to IN_PREPARATION
    storage_db_snapshot_status_t status = db->snapshot.status;
    if (!__atomic_compare_exchange_n(&db->snapshot.status, &status,
                                     STORAGE_DB_SNAPSHOT_STATUS_IN_PREPARATION, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        // The status was not set to IN_PREPARATION, so another thread is already preparing the snapshot, so we just
        // need to wait for it to be ready
        storage_db_snapshot_wait_for_prepared(db);
        return db->snapshot.status != STORAGE_DB_SNAPSHOT_STATUS_FAILED;
    }

    // Prepare the snapshot
    return storage_db_snapshot_rdb_prepare(db);
}

bool storage_db_snapshot_rdb_write_value_header(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index) {
    bool result = true;
    uint8_t buffer[128] = { 0 };
    size_t buffer_size = sizeof(buffer);
    size_t buffer_offset = 0;
    char *key = NULL;
    size_t key_length;
    bool key_allocated_new_buffer = false;
    storage_db_chunk_info_t *chunk_info;

    static module_redis_snapshot_value_type_t value_type_cg_to_rdb_map[] = {
            0, 0, // The first 2 values for cachegrand are not used
            MODULE_REDIS_SNAPSHOT_VALUE_TYPE_STRING
    };

    // Convert the cachegrand value type to the redis one
    assert(entry_index->value_type >= sizeof(value_type_cg_to_rdb_map) / sizeof(value_type_cg_to_rdb_map[0]));

    // Read the key
    chunk_info = storage_db_chunk_sequence_get(
            &entry_index->key,
            0);
    key = storage_db_get_chunk_data(db, chunk_info, &key_allocated_new_buffer);
    key_length = chunk_info->chunk_length;


    // Check if the entry has an expiration time
    if (entry_index->expiry_time_ms != 0) {
        // Serialize and write the expire time opcode
        if (module_redis_snapshot_serialize_primitive_encode_opcode_expire_time_ms(
                entry_index->expiry_time_ms,
                buffer,
                buffer_size,
                0,
                &buffer_offset) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
            LOG_E(TAG, "Failed to write the expire time opcode");
            result = false;
            goto end;
        }

        if (!storage_db_snapshot_rdb_write_buffer(db, buffer, buffer_offset)) {
            result = false;
            goto end;
        }
    }

    // Serialize and write the value type opcode
    if (module_redis_snapshot_serialize_primitive_encode_opcode_value_type(
            value_type_cg_to_rdb_map[entry_index->value_type],
            buffer,
            buffer_size,
            0,
            &buffer_offset) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
        LOG_E(TAG, "Failed to write the value type opcode");
        result = false;
        goto end;
    }

    if (!storage_db_snapshot_rdb_write_buffer(db, buffer, buffer_offset)) {
        result = false;
        goto end;
    }

    // Encode the key length
    if (module_redis_snapshot_serialize_primitive_encode_length(
            key_length,
            buffer,
            buffer_size,
            0,
            &buffer_offset) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
        LOG_E(TAG, "Failed to write the key length");
        result = false;
        goto end;
    }

    if (!storage_db_snapshot_rdb_write_buffer(db, buffer, buffer_offset)) {
        result = false;
        goto end;
    }

    // Write the key
    if (!storage_db_snapshot_rdb_write_buffer(db, (uint8_t*)key, key_length)) {
        result = false;
        goto end;
    }

end:
    // Free the key if it was allocated
    if (key_allocated_new_buffer) {
        ffma_mem_free(key);
    }

    return result;
}

bool storage_db_snapshot_rdb_write_value_string(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index) {
    uint8_t buffer[128] = { 0 };
    size_t buffer_size = sizeof(buffer);
    size_t buffer_offset = 0;
    int64_t string_integer;
    bool result = true;
    bool string_allocated_new_buffer = false;
    bool string_serialized = false;
    storage_db_chunk_info_t *chunk_info;
    module_redis_snapshot_serialize_primitive_result_t serialize_result;

    if (likely(entry_index->value.count == 1)) {
        // If the string is small enough, it can be encoded as a small string int so get the string, it's
        // always stored in the first chunk
        chunk_info = storage_db_chunk_sequence_get(
                &entry_index->value,
                0);

        char *string = storage_db_get_chunk_data(
                db,
                chunk_info,
                &string_allocated_new_buffer);

        if (unlikely(entry_index->value.size) <= 21) {
            if (module_redis_snapshot_serialize_primitive_can_encode_string_int(
                    string,
                    entry_index->value.size,
                    &string_integer)) {
                if (unlikely(module_redis_snapshot_serialize_primitive_encode_small_string_int(
                        string_integer,
                        buffer,
                        buffer_size,
                        0,
                        &buffer_offset) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK)) {
                    LOG_E(TAG, "Failed to write the small string int");
                    result = false;
                    goto end;
                }

                if (unlikely(!storage_db_snapshot_rdb_write_buffer(db, buffer, buffer_offset))) {
                    result = false;
                    goto end;
                }

                string_serialized = true;
            }
        }

        // If the string is longer than 32 chars or can't be serialized as integer, try to compress it
        if (likely(!string_serialized || entry_index->value.size > 32)) {
            size_t allocated_buffer_size = LZF_MAX_COMPRESSED_SIZE(entry_index->value.size);
            uint8_t *allocated_buffer = ffma_mem_alloc(allocated_buffer_size);

            serialize_result = module_redis_snapshot_serialize_primitive_encode_small_string_lzf(
                    string,
                    entry_index->value.size,
                    allocated_buffer,
                    allocated_buffer_size,
                    0,
                    &buffer_offset);

            // If the compression fails or the ration is too low ignore the error, cachegrand will try to
            // save the string as a regular string
            if (likely(serialize_result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK)) {
                if (unlikely(!storage_db_snapshot_rdb_write_buffer(
                        db,
                        allocated_buffer,
                        allocated_buffer_size))) {
                    ffma_mem_free(allocated_buffer);
                    result = false;
                    goto end;
                }

                string_serialized = true;
            }

            ffma_mem_free(allocated_buffer);
        }
    }

    // If the string hasn't been serialized via the fast path or is too long, serialize it as a regular
    // string
    if (unlikely(!string_serialized)) {
        // Encode the string length
        if (unlikely(module_redis_snapshot_serialize_primitive_encode_length(
                entry_index->value.size,
                buffer,
                buffer_size,
                0,
                &buffer_offset) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK)) {
            LOG_E(TAG, "Failed to write the string length");
            result = false;
            goto end;
        }

        if (unlikely(!storage_db_snapshot_rdb_write_buffer(db, buffer, buffer_offset))) {
            result = false;
            goto end;
        }

        for(uint32_t chunk_index = 0; chunk_index < entry_index->value.count; chunk_index++) {
            chunk_info = storage_db_chunk_sequence_get(
                    &entry_index->value,
                    chunk_index);

            char *string = storage_db_get_chunk_data(
                    db,
                    chunk_info,
                    &string_allocated_new_buffer);

            // Write the string
            if (unlikely(!storage_db_snapshot_rdb_write_buffer(
                    db,
                    (uint8_t*)string,
                    chunk_info->chunk_length))) {
                result = false;
                goto end;
            }

            // Free the string if it was allocated
            if (string_allocated_new_buffer) {
                ffma_mem_free(string);
            }
        }
    }

end:
    return result;
}

bool storage_db_snapshot_rdb_process_entry_index(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index) {
    // Write the header of the value
    if (!storage_db_snapshot_rdb_write_value_header(db, entry_index)) {
        return false;
    }

    // Depending on the value type, serialize the data
    switch(entry_index->value_type) {
        case STORAGE_DB_ENTRY_INDEX_VALUE_TYPE_STRING:
            if (!storage_db_snapshot_rdb_write_value_string(db, entry_index)) {
                return false;
            }
            break;

        default:
            FATAL(TAG, "Unsupported value type");
    }

    return true;
}

bool storage_db_snapshot_completed_successfully(
        storage_db_t *db) {
    // Close storage channel of the snapshot
    if (!storage_close(db->snapshot.storage_channel)) {
        return false;
    }

    // Swap atomically the temporary file with the main snapshot file
    if (rename(db->snapshot.path,db->config->snapshot.path) == -1) {
        LOG_E(TAG, "Failed to atomically rename the snapshot file");
        return false;
    }

    // Get the path to the parent directory
    char snapshot_path_tmp[PATH_MAX + 1];
    strcpy(snapshot_path_tmp, db->config->snapshot.path);
    char* snapshot_parent_path = dirname(snapshot_path_tmp);

    // Try to sync the directory to the disk to ensure that the metadata (e.g. the new location of the snapshot file,
    // its size, etc.) are all written to the disk but if the operation fails just report a warning
    int snapshot_parent_dir_fd = open(snapshot_parent_path, O_RDONLY);
    if (snapshot_parent_dir_fd != -1) {
        if (fsync(snapshot_parent_dir_fd) == -1) {
            LOG_W(
                    TAG,
                    "Failed to sync the directory <%s> to the disk, a crash might cause the corruption of the snapshot",
                    snapshot_parent_path);
            LOG_E_OS_ERROR(TAG);
        }

        close(snapshot_parent_dir_fd);
    } else {
        LOG_W(
                TAG,
                "Failed to open the directory <%s> to sync it to the disk, a crash might cause the corruption of the snapshot",
                snapshot_parent_path);
        LOG_E_OS_ERROR(TAG);
    }

    // The operation has completed successfully, update the internal structures
    storage_db_snapshot_completed(db, STORAGE_DB_SNAPSHOT_STATUS_COMPLETED);

    return true;
}

bool storage_db_snapshot_rdb_completed_successfully(
        storage_db_t *db) {
    uint8_t buffer[128] = { 0 };
    size_t buffer_size = sizeof(buffer);
    size_t buffer_offset = 0;

    // TODO: add the checksum
    if (module_redis_snapshot_serialize_primitive_encode_opcode_eof(
            0,
            buffer,
            buffer_size,
            0,
            &buffer_offset) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
        return false;
    }

    // Write the block to the snapshot file
    if (!storage_db_snapshot_rdb_write_buffer(db, buffer, buffer_offset)) {
        return false;
    }

    return storage_db_snapshot_completed_successfully(db);
}

bool storage_db_snapshot_rdb_process_entry_index_to_be_deleted_queue(
        storage_db_t *db) {
    bool result = true;
    storage_db_snapshot_entry_index_to_be_deleted_t *data = NULL;

    uint32_t counter = 0;
    while ((data = queue_mpmc_pop(db->snapshot.entry_index_to_be_deleted_queue)) != NULL &&
        counter++ < (STORAGE_DB_SNAPSHOT_BLOCK_SIZE / 2)) {
        // Process the entry index
        result = storage_db_snapshot_rdb_process_entry_index(
                db,
                data->key,
                data->key_length,
                data->entry_index);

        // If the operation is successful, update the statistics
        if (result) {
            db->snapshot.stats.keys_written++;
        }

        // Free the memory
        xalloc_free(data->key);
        ffma_mem_free(data);

        // If the operation failed, exit after freeing the memory
        if (!result) {
            break;
        }
    }

    return result;
}

void storage_db_snapshot_rdb_flush_entry_index_to_be_deleted_queue(
        storage_db_t *db) {
    storage_db_snapshot_entry_index_to_be_deleted_t *data;

    while ((data = queue_mpmc_pop(db->snapshot.entry_index_to_be_deleted_queue)) != NULL) {
        storage_db_entry_index_status_decrease_readers_counter(data->entry_index, NULL);
        xalloc_free(data->key);
        ffma_mem_free(data);
    }
}

bool storage_db_snapshot_rdb_process_block(
        storage_db_t *db,
        bool *last_block) {
    bool result = true;

    // Get the end of the hashtable
    uint64_t buckets_end = db->hashtable->ht_current->buckets_count_real;

    // Acquire a new snapshot block index and calculate the block start and block end
    uint64_t block_index = db->snapshot.block_index++;
    uint64_t block_start = block_index * STORAGE_DB_SNAPSHOT_BLOCK_SIZE;
    uint64_t block_end = block_start + STORAGE_DB_SNAPSHOT_BLOCK_SIZE;

    // Check if the block is the last one
    if (block_end >= buckets_end) {
        *last_block = true;
    }

    // Loop over the buckets in the block
    for (hashtable_bucket_index_t bucket_index = block_start;
         bucket_index < block_end && bucket_index < buckets_end;
         bucket_index++) {
        // Tries to fetch the next entry within the block being processed
        storage_db_entry_index_t *entry_index = (storage_db_entry_index_t*)hashtable_mcmp_op_iter_max_distance(
                db->hashtable,
                &bucket_index,
                buckets_end - bucket_index);

        // Prepare for the next iteration, if there is one
        bucket_index++;

        // Ensure the entry is not null
        if (entry_index == NULL) {
            // If the entry is NULL, it means that the block has been fully processed or the iterator has reached the
            // end of the hashtable, in both cases the loop can be terminated
            break;
        }

        // Check if the creation time is previous to the start time of the snapshot, in which case the key was created
        // after the snapshot was started and it should be skipped
        if (entry_index->created_time_ms < db->snapshot.start_time_ms) {
            continue;
        }

        // Get the entry
        entry_index = storage_db_get_entry_index_for_read_prep_no_expired_eviction(db, entry_index);
        if (unlikely(entry_index == NULL)) {
            // If entry_index is null after the call to storage_db_get_entry_index_for_read_prep_no_expired_eviction,
            // it means that it has been deleted or has expired, in this case the key can be skipped
            continue;
        }

        // Serialize the entry
        if (!storage_db_snapshot_rdb_process_entry_index(db, entry_index)) {
            result = false;
            break;
        }
    }

    if (result) {
        // If the block is the last one, wrap it up
        if (*last_block) {
            result = storage_db_snapshot_rdb_completed_successfully(db);
        }
    }

    if (result == false) {
        storage_db_snapshot_failed(db);
    }

    return result;
}
