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
#include "module/redis/snapshot/module_redis_snapshot.h"
#include "module/redis/snapshot/module_redis_snapshot_serialize_primitive.h"

#include "storage_db_snapshot.h"

#define TAG "storage_db_snapshot"

void storage_db_snapshot_rdb_release_slice(
        storage_db_t *db,
        size_t slice_used_length) {
    storage_buffered_write_buffer_release_slice(
            db->snapshot.storage_buffered_channel,
            slice_used_length);

    db->snapshot.stats.data_written += slice_used_length;
}

void storage_db_snapshot_completed(
        storage_db_t *db,
        storage_db_snapshot_status_t status) {
    char snapshot_duration_buffer[256] = { 0 };
    char next_shapshot_run_buffer[256] = { 0 };

    if (db->snapshot.path) {
        ffma_mem_free(db->snapshot.path);
    }

    // Update the snapshot general information, no need to check if the update next run time is fails as this function
    // is called only when the snapshot is completed or failed by the thread that is running the snapshot and all the
    // other threads will wait for the snapshot to be completed as this is the last block processed.
    storage_db_snapshot_update_next_run_time(db);
    db->snapshot.end_time_ms = clock_monotonic_coarse_int64_ms();
    db->snapshot.status = status;
    db->snapshot.path = NULL;
    db->snapshot.running = false;
    MEMORY_FENCE_STORE();

    assert(queue_mpmc_pop(db->snapshot.entry_index_to_be_deleted_queue) == NULL);

    // Calculate the snapshot duration and when the next snapshot will be run
    uint64_t snapshot_duration = db->snapshot.end_time_ms - db->snapshot.start_time_ms;
    clock_timespan_human_readable(
            snapshot_duration,
            snapshot_duration_buffer,
            sizeof(snapshot_duration_buffer) - 1,
            true,
            true);

    uint64_t next_run_in = db->snapshot.next_run_time_ms - clock_monotonic_coarse_int64_ms();
    clock_timespan_human_readable(
            next_run_in,
            next_shapshot_run_buffer,
            sizeof(next_shapshot_run_buffer) - 1,
            false,
            false);

    // Report the snapshot status
    if (status == STORAGE_DB_SNAPSHOT_STATUS_FAILED_DURING_PREPARATION) {
        LOG_E(TAG, "Snapshot failed during preparation");
    } else if (status == STORAGE_DB_SNAPSHOT_STATUS_FAILED) {
        LOG_E(TAG, "Snapshot failed after <%s>", snapshot_duration_buffer);
    } else if (status == STORAGE_DB_SNAPSHOT_STATUS_COMPLETED) {
        LOG_I(TAG, "Snapshot completed in <%s>", snapshot_duration_buffer);
    }
}

void storage_db_snapshot_failed(
        storage_db_t *db) {
    struct stat path_stat;

    // Close the snapshot file and delete the file from the disk
    if (db->snapshot.storage_channel_opened) {
        // Flush the buffer before closing the channel
        storage_buffered_flush_write(db->snapshot.storage_buffered_channel);
        storage_close(db->snapshot.storage_buffered_channel->storage_channel);
        db->snapshot.storage_channel_opened = false;
        MEMORY_FENCE_STORE();
        storage_buffered_channel_free(db->snapshot.storage_buffered_channel);
    }

    // Check if the snapshot file exists and, if yes, delete it
    if (db->snapshot.path != NULL && stat(db->snapshot.path, &path_stat) != -1) {
        unlink(db->snapshot.path);
    }

    // The operation has failed so the storage channel must be closed and the snapshot deleted
    storage_db_snapshot_completed(db, STORAGE_DB_SNAPSHOT_STATUS_FAILED_DURING_PREPARATION);
}

bool storage_db_snapshot_enough_keys_data_changed(
        storage_db_t *db) {
    storage_db_counters_t counters = { 0 };
    storage_db_config_t *config = db->config;

    // If the snapshot is running skip all the checks
    if (db->snapshot.running) {
        return true;
    }

    // If there are no constraints, the snapshot should run
    if (config->snapshot.min_keys_changed == 0 && config->snapshot.min_data_changed == 0) {
        return true;
    }

    // Get the current counters
    storage_db_counters_sum_global(db, &counters);

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

    return false;
}

bool storage_db_snapshot_should_run(
        storage_db_t *db) {
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

    return true;
}

void storage_db_snapshot_update_next_run_time(
        storage_db_t *db) {
    storage_db_config_t *config = db->config;
    db->snapshot.next_run_time_ms = clock_monotonic_coarse_int64_ms() + config->snapshot.interval_ms;
    db->snapshot.iteration++;
}

void storage_db_snapshot_skip_run(
        storage_db_t *db) {
    // Do nothing if another thread has already updated the next run time
    storage_db_snapshot_update_next_run_time(db);
}

void storage_db_snapshot_wait_for_prepared(
        storage_db_t *db) {
    // Wait for the snapshot to be ready
    do {
        MEMORY_FENCE_LOAD();
    } while (db->snapshot.status != STORAGE_DB_SNAPSHOT_STATUS_IN_PREPARATION);
}

void storage_db_snapshot_rdb_internal_status_reset(
        storage_db_t *db) {
    // Reset the status of the snapshot
    memset(&db->snapshot.stats, 0, sizeof(db->snapshot.stats));
    db->snapshot.storage_buffered_channel = NULL;
    db->snapshot.storage_channel_opened = false;
    db->snapshot.block_index = 0;
    db->snapshot.progress_reported_at_ms = clock_monotonic_int64_ms();
    db->snapshot.start_time_ms = 0;
    db->snapshot.end_time_ms = 0;
    db->snapshot.stats.keys_written = 0;
    db->snapshot.stats.data_written = 0;

    // Ensure the queue is empty
    assert(queue_mpmc_pop(db->snapshot.entry_index_to_be_deleted_queue) == NULL);
}

bool storage_db_snapshot_rdb_prepare(
        storage_db_t *db) {
    storage_io_common_fd_t snapshot_fd = 0;
    storage_buffered_channel_buffer_data_t *buffer;
    size_t buffer_size = 128;
    size_t buffer_offset = 0;
    char snapshot_path[PATH_MAX];
    struct stat parent_path_stat;
    storage_db_config_t *config = db->config;
    bool result = true;

    // Reset the status of the snapshot
    storage_db_snapshot_rdb_internal_status_reset(db);

    // Ensure that the parent directory of the path to be used for the snapshot exists and validates that it's readable
    // and writable
    char snapshot_path_tmp[PATH_MAX + 1];
    strcpy(snapshot_path_tmp, config->snapshot.path);
    char* parent_path = dirname(snapshot_path_tmp);
    if (stat(parent_path, &parent_path_stat) == -1) {
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
    storage_channel_t *storage_channel = NULL;
    if ((storage_channel = storage_open_fd(snapshot_fd)) == NULL) {
        LOG_E(TAG, "Unable to open the snapshot file descriptor");
        result = false;
        goto end;
    }

    // Wrap the storage channel in a storage buffered channel and mark it as opened
    db->snapshot.storage_buffered_channel = storage_buffered_channel_new(storage_channel);
    db->snapshot.storage_channel_opened = true;

    // Duplicate the path of the storage channel
    db->snapshot.path = ffma_mem_alloc_zero(db->snapshot.storage_buffered_channel->storage_channel->path_len + 1);
    strncpy(
            db->snapshot.path,
            db->snapshot.storage_buffered_channel->storage_channel->path,
            db->snapshot.storage_buffered_channel->storage_channel->path_len);

    // Acquire a slice of the buffer
    if ((buffer = storage_buffered_write_buffer_acquire_slice(
            db->snapshot.storage_buffered_channel,
            buffer_size)) == NULL) {
        LOG_E(TAG, "Failed to acquire a slice for the snapshot header");
        result = false;
        goto end;
    }

    // Write the snapshot header
    module_redis_snapshot_header_t header = { .version = STORAGE_DB_SNAPSHOT_RDB_VERSION };
    if (module_redis_snapshot_serialize_primitive_encode_header(
            &header,
            (uint8_t*)buffer,
            buffer_size,
            0,
            &buffer_offset) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
        LOG_E(TAG, "Failed to prepare the snapshot header");
        result = false;
        goto end;
    }

    // Release the buffer slice
    storage_db_snapshot_rdb_release_slice(db, buffer_offset);

    // Set the database number to zero and write it
    db->snapshot.current_database_number = 0;
    if (!storage_db_snapshot_rdb_write_database_number(db, db->snapshot.current_database_number)) {
        result = false;
        goto end;
    }

end:
    if (result) {
        storage_db_counters_t counters;
        storage_db_counters_sum_global(db, &counters);

        // The snapshot has been successfully prepared, so we can set running to true and update the status to
        // IN_PROGRESS
        db->snapshot.start_time_ms = clock_monotonic_coarse_int64_ms();
        db->snapshot.keys_changed_at_start = counters.keys_changed;
        db->snapshot.data_changed_at_start = counters.data_changed;
        db->snapshot.status = STORAGE_DB_SNAPSHOT_STATUS_IN_PROGRESS;
        MEMORY_FENCE_STORE();

        db->snapshot.running = true;
        MEMORY_FENCE_STORE();

        LOG_I(TAG, "Snapshot started");
    } else {
        storage_db_snapshot_failed(db);

        // Doesn't matter if the close fails, it might have been closed already, it's pointless to check we can just
        // ignore the error if there is one
        if (snapshot_fd > 0) {
            close(snapshot_fd);
        }
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
    // thread, so we just need to wait for it to be ready
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
        char *key,
        size_t key_length,
        storage_db_entry_index_t *entry_index) {
    bool result = true;
    bool buffer_can_contain_key = false;
    storage_buffered_channel_buffer_data_t *buffer;
    size_t buffer_size = 128;
    size_t buffer_offset = 0;

    if (buffer_size + key_length < db->snapshot.storage_buffered_channel->buffers.write.buffer.length) {
        buffer_can_contain_key = true;
        buffer_size += key_length;
    }

    // Acquire a slice of the buffer
    if ((buffer = storage_buffered_write_buffer_acquire_slice(
            db->snapshot.storage_buffered_channel,
            buffer_size)) == NULL) {
        LOG_E(TAG, "Failed to acquire a slice for the value header");
        result = false;
        goto end;
    }

    // Check if the entry has an expiration time
    if (entry_index->expiry_time_ms != 0) {
        // Serialize and write the expiry time opcode
        if (module_redis_snapshot_serialize_primitive_encode_opcode_expire_time_ms(
                entry_index->expiry_time_ms,
                (uint8_t*)buffer,
                buffer_size,
                buffer_offset,
                &buffer_offset) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
            LOG_E(TAG, "Failed to write the expire time opcode");
            result = false;
            goto end;
        }
    }

    // Serialize and write the value type opcode
    if (module_redis_snapshot_serialize_primitive_encode_opcode_value_type(
            MODULE_REDIS_SNAPSHOT_VALUE_TYPE_STRING,
            (uint8_t*)buffer,
            buffer_size,
            buffer_offset,
            &buffer_offset) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
        LOG_E(TAG, "Failed to write the value type opcode");
        result = false;
        goto end;
    }

    // Encode the key length
    if (module_redis_snapshot_serialize_primitive_encode_length(
            key_length,
            (uint8_t*)buffer,
            buffer_size,
            buffer_offset,
            &buffer_offset) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
        LOG_E(TAG, "Failed to write the key length");
        result = false;
        goto end;
    }

    if (!buffer_can_contain_key) {
        // Release the current slice and acquire a new one for the key
        storage_db_snapshot_rdb_release_slice(db,buffer_offset);

        // Acquire a slice of the buffer
        if ((buffer = storage_buffered_write_buffer_acquire_slice(
                db->snapshot.storage_buffered_channel,
                key_length)) == NULL) {
            LOG_E(TAG, "Failed to acquire a slice for the key");
            result = false;
            goto end;
        }

        buffer_offset = 0;
    }

    // Copy the key onto the destination buffer
    memcpy(buffer + buffer_offset, key, key_length);
    buffer_offset += key_length;

    // Release the current slice
    storage_db_snapshot_rdb_release_slice(db, buffer_offset);

end:
    return result;
}

bool storage_db_snapshot_rdb_write_value_string(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index) {
    storage_buffered_channel_buffer_data_t *buffer;
    size_t buffer_size;
    size_t buffer_offset = 0;
    int64_t string_integer;
    bool result = true;
    bool string_allocated_new_buffer = false;
    bool string_serialized = false;
    storage_db_chunk_info_t *chunk_info;

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
                // Acquire a slice of the buffer
                buffer_size = 128;
                if ((buffer = storage_buffered_write_buffer_acquire_slice(
                        db->snapshot.storage_buffered_channel,
                        buffer_size)) == NULL) {
                    LOG_E(TAG, "Failed to acquire a slice for the small string as int");
                    result = false;
                    goto end;
                }

                if (unlikely(module_redis_snapshot_serialize_primitive_encode_small_string_int(
                        string_integer,
                        (uint8_t*)buffer,
                        buffer_size,
                        0,
                        &buffer_offset) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK)) {
                    LOG_E(TAG, "Failed to write the small string int");
                    result = false;
                    goto end;
                }

                // Release the slice
                storage_db_snapshot_rdb_release_slice(db, buffer_offset);
                string_serialized = true;
            }
        }

//        // Try to compress the string using LZF
//        if (likely(!string_serialized && (entry_index->value.size > 32 && entry_index->value.size < 40 * 1024))) {
//            module_redis_snapshot_serialize_primitive_result_t serialize_result;
//            buffer_size = 128 + (size_t)(LZF_MAX_COMPRESSED_SIZE(entry_index->value.size) * 1.2);
//            if ((buffer = storage_buffered_write_buffer_acquire_slice(
//                    db->snapshot.storage_buffered_channel,
//                    buffer_size)) == NULL) {
//                LOG_E(TAG, "Failed to acquire a slice for the string value");
//                result = false;
//                goto end;
//            }
//
//            serialize_result = module_redis_snapshot_serialize_primitive_encode_small_string_lzf(
//                    string,
//                    entry_index->value.size,
//                    (uint8_t*)buffer,
//                    buffer_size,
//                    0,
//                    &buffer_offset);
//
//            // If the compression fails or the ration is too low ignore the error, cachegrand will try to
//            // save the string as a regular string
//            if (likely(serialize_result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK)) {
//                storage_db_snapshot_rdb_release_slice(db, buffer_offset);
//                string_serialized = true;
//            } else {
//                storage_buffered_write_buffer_discard_slice(db->snapshot.storage_buffered_channel);
//            }
//        }
    }

    // If the string hasn't been serialized via the fast path or is too long, serialize it as a regular
    // string
    if (unlikely(!string_serialized)) {
        // Acquire a slice of the buffer to write the string length
        buffer_size = 128;
        if ((buffer = storage_buffered_write_buffer_acquire_slice(
                db->snapshot.storage_buffered_channel,
                buffer_size)) == NULL) {
            LOG_E(TAG, "Failed to acquire a slice for the string value");
            result = false;
            goto end;
        }

        // Encode the string length
        if (unlikely(module_redis_snapshot_serialize_primitive_encode_length(
                entry_index->value.size,
                (uint8_t*)buffer,
                buffer_size,
                0,
                &buffer_offset) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK)) {
            LOG_E(TAG, "Failed to write the string length");
            result = false;
            goto end;
        }

        // Release the slice
        storage_db_snapshot_rdb_release_slice(db, buffer_offset);

        for(uint32_t chunk_index = 0; chunk_index < entry_index->value.count; chunk_index++) {
            chunk_info = storage_db_chunk_sequence_get(
                    &entry_index->value,
                    chunk_index);

            char *string = storage_db_get_chunk_data(
                    db,
                    chunk_info,
                    &string_allocated_new_buffer);

            if (!string) {
                LOG_E(TAG, "Failed to read the chunk data");
                result = false;
                goto end;
            }

            // Acquire a slice of the buffer
            if ((buffer = storage_buffered_write_buffer_acquire_slice(
                    db->snapshot.storage_buffered_channel,
                    chunk_info->chunk_length)) == NULL) {
                LOG_E(TAG, "Failed to acquire a slice for the string data");
                result = false;
                goto end;
            }

            memcpy(buffer, string, chunk_info->chunk_length);

            // Release the slice
            storage_db_snapshot_rdb_release_slice(db, chunk_info->chunk_length);

            // Free the string if it was allocated
            if (string_allocated_new_buffer) {
                ffma_mem_free(string);
            }
        }
    }

end:
    return result;
}

bool storage_db_snapshot_rdb_write_database_number(
        storage_db_t *db,
        storage_db_database_number_t database_number) {
    bool result = true;
    storage_buffered_channel_buffer_data_t *buffer;
    size_t buffer_size = 128;
    size_t buffer_offset = 0;

    // Acquire a slice of the buffer
    if ((buffer = storage_buffered_write_buffer_acquire_slice(
            db->snapshot.storage_buffered_channel,
            buffer_size)) == NULL) {
        LOG_E(TAG, "Failed to acquire a slice for the database number");
        result = false;
        goto end;
    }

    if (module_redis_snapshot_serialize_primitive_encode_opcode_db_number(
            database_number,
            (uint8_t*)buffer,
            buffer_size,
            0,
            &buffer_offset) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
        LOG_E(TAG, "Failed to write the database number");
        result = false;
        goto end;
    }

    // Release the slice
    storage_db_snapshot_rdb_release_slice(db, buffer_offset);

end:

    return result;
}

bool storage_db_snapshot_rdb_process_entry_index(
        storage_db_t *db,
        char *key,
        size_t key_size,
        storage_db_entry_index_t *entry_index) {
    bool result = true;

    // Check if the snapshot time of the entry is after the start of the snapshot process
    if (entry_index->snapshot_time_ms > db->snapshot.start_time_ms) {
        // If the snapshot time of the entry is newer than the snapshot start time, it means that the entry has been
        // deleted or modified and pushed in the queue after the snapshot process has started. Therefore, it has already
        // been serialized and we can skip it
        return true;
    }

    // Write the header of the value
    if (!storage_db_snapshot_rdb_write_value_header(
            db,
            key,
            key_size,
            entry_index)) {
        result = false;
        goto end;
    }

    // Depending on the value type, serialize the data
    assert(entry_index->value_type == 0);
    if (!storage_db_snapshot_rdb_write_value_string(db, entry_index)) {
        result = false;
        goto end;
    }

end:
    // To spare resources, set the snapshot time to the start time plus one, no need to use memory fences as this
    // process can't be executed in parallel as the RDB file format doesn't allow it
    entry_index->snapshot_time_ms = db->snapshot.start_time_ms + 1;

    // Decrease the number of readers of the entry index
    storage_db_entry_index_status_decrease_readers_counter(entry_index, NULL);
    return result;
}

void storage_db_snapshot_mark_as_being_finalized(
        storage_db_t *db) {
    db->snapshot.status = STORAGE_DB_SNAPSHOT_STATUS_BEING_FINALIZED;
    MEMORY_FENCE_STORE();
}

bool storage_db_snapshot_completed_successfully(
        storage_db_t *db) {
    // Flush the buffer before closing the channel
    storage_buffered_flush_write(db->snapshot.storage_buffered_channel);

    // Close storage channel of the snapshot
    if (!storage_close(db->snapshot.storage_buffered_channel->storage_channel)) {
        return false;
    }

    db->snapshot.storage_channel_opened = false;
    storage_buffered_channel_free(db->snapshot.storage_buffered_channel);
    MEMORY_FENCE_STORE();

    if (db->config->snapshot.rotation_max_files > 1) {
        // Rotate the snapshot files, file_index will never be bigger than UINT16_MAX as the value is validated when
        // the configuration is loaded
        for(int32_t index = (int32_t)db->config->snapshot.rotation_max_files - 1; index >= 0; index--) {
            char snapshot_path_rotated[PATH_MAX + 1];

            if (index == 0) {
                strcpy(
                        snapshot_path_rotated,
                        db->config->snapshot.path);
            } else {
                snprintf(
                        snapshot_path_rotated,
                        PATH_MAX,
                        "%s.%d",
                        db->config->snapshot.path,
                        index);
            }

            char snapshot_path_rotated_next[PATH_MAX + 1];
            snprintf(
                    snapshot_path_rotated_next,
                    PATH_MAX,
                    "%s.%d", db->config->snapshot.path,
                    index + 1);

            if (rename(snapshot_path_rotated, snapshot_path_rotated_next) == -1) {
                if (errno != ENOENT) {
                    LOG_E(TAG, "Failed to rotate the snapshot file");
                    LOG_E_OS_ERROR(TAG);
                    return false;
                }
            }
        }
    }

    // Swap atomically the temporary file with the main snapshot file
    if (rename(db->snapshot.path, db->config->snapshot.path) == -1) {
        LOG_E(TAG, "Failed to atomically rename the snapshot file");
        LOG_E_OS_ERROR(TAG);
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
    storage_buffered_channel_buffer_data_t *buffer;
    size_t buffer_size = 128;
    size_t buffer_offset = 0;

    // Acquire a slice of the buffer
    if ((buffer = storage_buffered_write_buffer_acquire_slice(
            db->snapshot.storage_buffered_channel,
            buffer_size)) == NULL) {
        LOG_E(TAG, "Failed to acquire a slice for the snapshot end of file");
        return false;
    }

    // TODO: add the checksum
    if (module_redis_snapshot_serialize_primitive_encode_opcode_eof(
            0,
            (uint8_t*)buffer,
            buffer_size,
            0,
            &buffer_offset) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
        return false;
    }

    // Release the slice
    storage_db_snapshot_rdb_release_slice(db, buffer_offset);

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
        storage_db_database_number_t *current_database_number,
        bool *last_block) {
    bool result = true;

    // Get the end of the hashtable
    uint64_t buckets_end = db->hashtable->ht_current->buckets_count_real;

    // Acquire a new block index and calculate the start and the end
    uint64_t block_index = db->snapshot.block_index++;
    MEMORY_FENCE_STORE();
    uint64_t block_start = block_index * STORAGE_DB_SNAPSHOT_BLOCK_SIZE;
    uint64_t block_end = block_start + STORAGE_DB_SNAPSHOT_BLOCK_SIZE;

    // Check if the block is the last one
    *last_block = false;
    if (block_end >= buckets_end) {
        *last_block = true;
    }

    // Loop over the buckets in the block
    for (hashtable_bucket_index_t bucket_index = block_start - 1;
         bucket_index < block_end && bucket_index < buckets_end;
         bucket_index++) {
        storage_db_database_number_t database_number;
        char *key = NULL;
        hashtable_key_length_t key_size = 0;

        // Tries to fetch the next entry within the block being processed
        bucket_index++;
        storage_db_entry_index_t *entry_index =
                (storage_db_entry_index_t*)hashtable_mcmp_op_iter_max_distance_all_databases(
                        db->hashtable,
                        &bucket_index,
                        block_end - bucket_index);

        // Ensure the entry is not null
        if (entry_index == NULL) {
            // If the entry is NULL, it means that the block has been fully processed or the iterator has reached the
            // end of the hashtable, in both cases the loop can be terminated
            break;
        }

        // Check if the creation time is previous to the start time of the snapshot, in which case the key was created
        // after the snapshot was started and it should be skipped
        if (!storage_db_snapshot_should_entry_index_be_processed_creation_time(db, entry_index)) {
            continue;
        }

        // Try to get the key
        if (unlikely(!hashtable_mcmp_op_get_key_all_databases(
                db->hashtable,
                bucket_index,
                &database_number,
                &key,
                &key_size))) {
            continue;
        }

        // Get the entry
        entry_index = storage_db_get_entry_index_for_read_prep_no_expired_eviction(db, entry_index);
        if (unlikely(entry_index == NULL)) {
            // If entry_index is null after the call to storage_db_get_entry_index_for_read_prep_no_expired_eviction,
            // it means that it has been deleted or has expired, in this case the entry can be skipped and the key can
            // be freed
            xalloc_free(key);
            continue;
        }

        // Check if the database number has changed
        if (unlikely(*current_database_number != database_number)) {
            // If the database number has changed, the current database number should be updated
            *current_database_number = database_number;

            if (!storage_db_snapshot_rdb_write_database_number(db, database_number)) {
                result = false;
                goto loop_end;
            }
        }

        // Serialize the entry
        if (!storage_db_snapshot_rdb_process_entry_index(
                db,
                key,
                key_size,
                entry_index)) {
            result = false;
            goto loop_end;
        }

        db->snapshot.stats.keys_written++;

loop_end:
        // Free the key
        xalloc_free(key);

        if (unlikely(!result)) {
            break;
        }
    }

    return result;
}

void storage_db_snapshot_report_progress(
        storage_db_t *db) {
    char eta_buffer[CLOCK_TIMESPAN_MAX_LENGTH];

    uint64_t now_ms = clock_monotonic_int64_ms();

    // Calculate the block index end and ensure it is not greater than the buckets count
    uint64_t processed_block_index_end = db->snapshot.block_index * STORAGE_DB_SNAPSHOT_BLOCK_SIZE;
    if (processed_block_index_end > db->hashtable->ht_current->buckets_count_real) {
        processed_block_index_end = db->hashtable->ht_current->buckets_count_real;
    }

    // Calculate progress
    double progress =
            (double)processed_block_index_end *
            100.0 /
            (double)db->hashtable->ht_current->buckets_count_real;

    // Calculate the eta
    uint64_t eta_ms = 0;
    if (progress > 0) {
        eta_ms = (uint64_t)(((double)now_ms - (double)db->snapshot.start_time_ms) * ((100.0 - progress) / progress));
    }

    // Report the progress
    LOG_I(
            TAG,
            "Snapshot progress <%0.02f%%>, keys processed <%lu>, data written <%0.02lf MB>, eta: <%s>",
            progress,
            db->snapshot.stats.keys_written,
            (double)db->snapshot.stats.data_written / 1024.0 / 1024.0,
            clock_timespan_human_readable(
                    eta_ms,
                    eta_buffer,
                    sizeof(eta_buffer),
                    false,
                    false));

    // Update the progress reported at time
    db->snapshot.progress_reported_at_ms = clock_monotonic_int64_ms();
}
