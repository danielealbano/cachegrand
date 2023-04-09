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
#include "data_structures/hashtable/mcmp/hashtable_op_get_key.h"
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

    db->snapshot.stats.data_written += buffer_size;
    db->snapshot.offset += (off_t)buffer_size;

    return true;
}

void storage_db_snapshot_completed(
        storage_db_t *db,
        storage_db_snapshot_status_t status) {
    // Update the snapshot general information
    storage_db_snapshot_update_next_run_time(db);
    db->snapshot.end_time_ms = clock_monotonic_coarse_int64_ms();
    db->snapshot.status = status;
    db->snapshot.path = NULL;

    // Report the result of the snapshot
    LOG_I(
            TAG,
            status == STORAGE_DB_SNAPSHOT_STATUS_COMPLETED
            ? "Snapshot completed in <%lu> ms"
            : "Snapshot failed after <%lu> ms",
            db->snapshot.end_time_ms - db->snapshot.start_time_ms);
    LOG_I(TAG, "Next snapshot in <%lu> s", (db->snapshot.next_run_time_ms - db->snapshot.end_time_ms) / 1000);

    // Sync the data
    db->snapshot.running = false;
    MEMORY_FENCE_STORE();

    assert(queue_mpmc_pop(db->snapshot.entry_index_to_be_deleted_queue) == NULL);
}

void storage_db_snapshot_failed(
        storage_db_t *db) {
    struct stat path_stat;

    // Close the snapshot file and delete the file from the disk
    if (db->snapshot.storage_channel_opened) {
        storage_close(db->snapshot.storage_channel);
        db->snapshot.storage_channel_opened = false;
        MEMORY_FENCE_STORE();
    }

    // Check if the snapshot file exists and, if yes, delete it
    if (db->snapshot.path != NULL && stat(db->snapshot.path, &path_stat) != -1) {
        unlink(db->snapshot.path);
    }

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

void storage_db_snapshot_rdb_internal_status_reset(
        storage_db_t *db) {
    // Reset the status of the snapshot
    memset(&db->snapshot.stats, 0, sizeof(db->snapshot.stats));
    db->snapshot.storage_channel = NULL;
    db->snapshot.storage_channel_opened = false;
    db->snapshot.block_index = 0;
    db->snapshot.offset = 0;
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
    storage_io_common_fd_t snapshot_fd;
    char snapshot_path[PATH_MAX];
    struct stat parent_path_stat;
    uint8_t buffer[128] = { 0 };
    size_t buffer_size = sizeof(buffer);
    size_t buffer_offset = 0;
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
    if ((db->snapshot.storage_channel = storage_open_fd(snapshot_fd)) == NULL) {
        LOG_E(TAG, "Unable to open the snapshot file descriptor");
        result = false;
        goto end;
    }

    // Mark the storage channel as opened
    db->snapshot.storage_channel_opened = true;

    // Duplicate the path of the storage channel
    db->snapshot.path = ffma_mem_alloc_zero(db->snapshot.storage_channel->path_len + 1);
    strncpy(db->snapshot.path, db->snapshot.storage_channel->path, db->snapshot.storage_channel->path_len);

    // Write the snapshot header
    module_redis_snapshot_header_t header = { .version = STORAGE_DB_SNAPSHOT_RDB_VERSION };
    if (module_redis_snapshot_serialize_primitive_encode_header(
            &header,
            buffer,
            buffer_size,
            0,
            &buffer_offset) != MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK) {
        LOG_E(TAG, "Failed to prepare the snapshot header");
        result = false;
        goto end;
    }

    if (!storage_db_snapshot_rdb_write_buffer(db, buffer, buffer_offset)) {
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
        close(snapshot_fd);
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
        char *key,
        size_t key_length,
        storage_db_entry_index_t *entry_index) {
    bool result = true;
    uint8_t buffer[128] = { 0 };
    size_t buffer_size = sizeof(buffer);
    size_t buffer_offset = 0;

    static module_redis_snapshot_value_type_t value_type_cg_to_rdb_map[] = {
            0, 0, // The first 2 values for cachegrand are not used
            MODULE_REDIS_SNAPSHOT_VALUE_TYPE_STRING
    };

    // Convert the cachegrand value type to the redis one
    assert(entry_index->value_type >= sizeof(value_type_cg_to_rdb_map) / sizeof(value_type_cg_to_rdb_map[0]));

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

        // If the string is longer than 32 chars or can't be serialized as integer, try to compress it. It limits the
        // amount of data allowed to be compressed to 52kb to be sure to have room for extra space as
        // LZF_MAX_COMPRESSED_SIZE might return 104% and we are adding an additional 10% to be sure to have enough
        // space to compress the string
        if (false && likely(!string_serialized || (entry_index->value.size > 32 && entry_index->value.size < 52 * 1024))) {
            size_t allocated_buffer_size = LZF_MAX_COMPRESSED_SIZE(entry_index->value.size) * 1.2;
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
        char *key,
        size_t key_size,
        storage_db_entry_index_t *entry_index) {
    bool result = true;

    // Check if the snapshot time of the entry is after the start of the snapshot process
    if (entry_index->snapshot_time_ms > db->snapshot.start_time_ms) {
        // If the snapshot time of the entry is newer than the snapshot start time, it means that the entry has been
        // deleted or modified and pushed in the queue after the snapshot process has started. Therefore it has already
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
    // Close storage channel of the snapshot
    if (!storage_close(db->snapshot.storage_channel)) {
        return false;
    }

    db->snapshot.storage_channel_opened = false;
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

            // Check if the snapshot_path_rotated_next file exists
            if (access(snapshot_path_rotated, F_OK) != 0) {
                // If not, skip the file
                continue;
            }

            if (rename(snapshot_path_rotated, snapshot_path_rotated_next) == -1) {
                LOG_E(TAG, "Failed to rotate the snapshot file");
                LOG_E_OS_ERROR(TAG);
                return false;
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
        char *key = NULL;
        hashtable_key_size_t key_size = 0;

        // Tries to fetch the next entry within the block being processed
        bucket_index++;
        storage_db_entry_index_t *entry_index = (storage_db_entry_index_t*)hashtable_mcmp_op_iter_max_distance(
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
        if (unlikely(!hashtable_mcmp_op_get_key(db->hashtable, bucket_index, &key, &key_size))) {
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

        // Serialize the entry
        if (!storage_db_snapshot_rdb_process_entry_index(
                db,
                key,
                key_size,
                entry_index)) {
            result = false;
            xalloc_free(key);
            break;
        }

        db->snapshot.stats.keys_written++;

        // Free the key
        xalloc_free(key);
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
            "Snapshot progress <%0.02f%%>, keys processed <%lu>, data processed <%0.02lf MB>, eta: <%s>",
            progress,
            db->snapshot.stats.keys_written,
            (double)db->snapshot.stats.data_written / 1024.0 / 1024.0,
            clock_timespan_human_readable(
                    eta_ms,
                    eta_buffer,
                    sizeof(eta_buffer)));

    // Update the progress reported at time
    db->snapshot.progress_reported_at_ms = clock_monotonic_int64_ms();
}
