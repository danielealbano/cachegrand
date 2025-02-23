/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

// The macro STORAGE_DB_COUNTERS_UPDATE relies on running a function, passed as VA_ARGS, twice to update both the
// global counters and the per database counters, therefore the bugprone-macro-repeated-side-effects warning must be
// ignored as this is an intended behaviour.
#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-macro-repeated-side-effects"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <fcntl.h>
#include <pow2.h>
#include <stdatomic.h>
#include <assert.h>
#include <sys/stat.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "transaction.h"
#include "utils_string.h"
#include "xalloc.h"
#include "random.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"
#include "data_structures/hashtable/mcmp/hashtable_op_get.h"
#include "data_structures/hashtable/mcmp/hashtable_op_get_key.h"
#include "data_structures/hashtable/mcmp/hashtable_op_set.h"
#include "data_structures/hashtable/mcmp/hashtable_op_delete.h"
#include "data_structures/hashtable/mcmp/hashtable_op_iter.h"
#include "data_structures/hashtable/mcmp/hashtable_op_rmw.h"
#include "data_structures/hashtable/mcmp/hashtable_op_get_random_key.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "fiber/fiber.h"
#include "fiber/fiber_scheduler.h"
#include "log/log.h"
#include "config.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/storage.h"

#include "storage_db.h"
#include "storage_db_snapshot.h"
#include "storage_db_counters.h"

#define TAG "storage_db"


char *storage_db_shard_build_path(
        char *basedir_path,
        storage_db_shard_index_t shard_index) {
    char *path_template = "%.*s/db-%d.shard";
    char *path;
    size_t required_length;

    int basedir_path_len = (int)strlen(basedir_path);
    if (basedir_path[basedir_path_len - 1] == '/') {
        basedir_path_len--;
    }

    required_length = snprintf(
            NULL,
            0,
            path_template,
            basedir_path_len,
            basedir_path,
            shard_index);
    path = xalloc_alloc(required_length + 1);

    snprintf(
            path,
            required_length + 1,
            path_template,
            basedir_path_len,
            basedir_path,
            shard_index);

    return path;
}

storage_db_config_t* storage_db_config_new() {
    return xalloc_alloc_zero(sizeof(storage_db_config_t));
}

void storage_db_config_free(
        storage_db_config_t* config) {
    xalloc_free(config);
}

storage_db_t* storage_db_new(
        storage_db_config_t *config,
        uint32_t workers_count) {
    hashtable_config_t* hashtable_config = NULL;
    hashtable_t *hashtable = NULL;
    storage_db_worker_t *workers = NULL;
    storage_db_t *db = NULL;

    // Initialize the hashtable configuration
    hashtable_config = hashtable_mcmp_config_init();
    if (!hashtable_config) {
        LOG_E(TAG, "Unable to allocate memory for the hashtable configuration");
        goto fail;
    }
    hashtable_config->can_auto_resize = false;
    hashtable_config->initial_size = pow2_next(config->limits.keys_count.hard_limit);

    // Initialize the hashtable
    hashtable = hashtable_mcmp_init(hashtable_config);
    if (!hashtable) {
        LOG_E(TAG, "Unable to allocate memory for the hashtable");
        goto fail;
    }

    // Initialize the per-worker set of information
    workers = xalloc_alloc_zero(sizeof(storage_db_worker_t) * workers_count);
    if (!workers) {
        LOG_E(TAG, "Unable to allocate memory for the per worker configurations");
        goto fail;
    }

    // Initialize the per worker needed information
    for(uint32_t worker_index = 0; worker_index < workers_count; worker_index++) {
        ring_bounded_queue_spsc_voidptr_t *deleted_entry_index_ring_buffer =
                ring_bounded_queue_spsc_voidptr_init(STORAGE_DB_WORKER_ENTRY_INDEX_RING_BUFFER_SIZE);

        if (!deleted_entry_index_ring_buffer) {
            LOG_E(TAG, "Unable to allocate memory for the deleted entry index ring buffer per worker");
            goto fail;
        }

        workers[worker_index].deleted_entry_index_ring_buffer = deleted_entry_index_ring_buffer;

        double_linked_list_t *deleting_entry_index_list = double_linked_list_init();

        if (!deleting_entry_index_list) {
            LOG_E(TAG, "Unable to allocate memory for the deleting entry index ring buffer per worker");
            goto fail;
        }

        workers[worker_index].deleting_entry_index_list = deleting_entry_index_list;
    }

    // Initialize the db wrapper structure
    db = xalloc_alloc_zero(sizeof(storage_db_t));
    if (!db) {
        LOG_E(TAG, "Unable to allocate memory for the storage db");
        goto fail;
    }

    // Sets up all the db related information
    db->config = config;
    db->workers = workers;
    db->workers_count = workers_count;
    db->hashtable = hashtable;
    db->counters_slots_bitmap = slots_bitmap_mpmc_init(STORAGE_DB_WORKERS_MAX);
    db->snapshot.next_run_time_ms = 0;
    db->snapshot.status = STORAGE_DB_SNAPSHOT_STATUS_NONE;
    db->snapshot.in_preparation = false;
    db->snapshot.block_index = 0;
    db->snapshot.storage_channel_opened = false;
    db->snapshot.storage_buffered_channel = NULL;
    db->snapshot.path = NULL;
    db->snapshot.running = false;
    db->snapshot.keys_changed_at_start = 0;
    db->snapshot.data_changed_at_start = 0;
    db->snapshot.entry_index_to_be_deleted_queue = queue_mpmc_init();

    spinlock_init(&db->snapshot.spinlock);

    // Sets up the shards only if it has to write to the disk
    if (config->backend_type != STORAGE_DB_BACKEND_TYPE_MEMORY) {
        db->shards.new_index = 0;
        spinlock_init(&db->shards.write_spinlock);
        db->shards.opened_shards = double_linked_list_init();

        if (!db->shards.opened_shards) {
            LOG_E(TAG, "Unable to allocate for the list of opened shards");
            goto fail;
        }
    }

    // Import the hard and soft limits for the keys eviction
    memcpy(&db->limits, &config->limits, sizeof(storage_db_config_limits_t));

    return db;

fail:
    if (hashtable) {
        hashtable_mcmp_free(hashtable);
        hashtable_config = NULL;
    }

    if (hashtable_config) {
        hashtable_mcmp_config_free(hashtable_config);
    }

    if (workers) {
        for(uint32_t worker_index = 0; worker_index < workers_count; worker_index++) {
            if (workers[worker_index].deleted_entry_index_ring_buffer) {
                ring_bounded_queue_spsc_voidptr_free(workers[worker_index].deleted_entry_index_ring_buffer);
            }

            if (workers[worker_index].deleting_entry_index_list) {
                double_linked_list_free(workers[worker_index].deleting_entry_index_list);
            }
        }

        xalloc_free(workers);
    }

    if (db) {
        // This can't really happen with the current implementation but better to have it in place to avoid future
        // bugs caused by code refactorings
        if (db->shards.opened_shards) {
            double_linked_list_free(db->shards.opened_shards);
        }

        xalloc_free(db);
    }

    return NULL;
}

storage_channel_t *storage_db_shard_open_or_create_file(
        char *path,
        bool create) {
    storage_channel_t *storage_channel = storage_open(
            path,
            (create ? (O_CREAT) : 0) | O_RDWR,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

    return storage_channel;
}

bool storage_db_shard_ensure_size_pre_allocated(
        storage_channel_t *storage_channel,
        uint32_t size_mb) {
    return storage_fallocate(storage_channel, 0, 0, size_mb * 1024 * 1024);
}

storage_db_shard_t *storage_db_worker_active_shard(
        storage_db_t *db) {
    worker_context_t *worker_context = worker_context_get();
    uint32_t worker_index = worker_context->worker_index;

    return db->workers[worker_index].active_shard;
}

ring_bounded_queue_spsc_voidptr_t *storage_db_worker_deleted_entry_index_ring_buffer(
        storage_db_t *db) {
    worker_context_t *worker_context = worker_context_get();
    uint32_t worker_index = worker_context->worker_index;

    return db->workers[worker_index].deleted_entry_index_ring_buffer;
}

double_linked_list_t *storage_db_worker_deleting_entry_index_list(
        storage_db_t *db) {
    worker_context_t *worker_context = worker_context_get();
    uint32_t worker_index = worker_context->worker_index;

    return db->workers[worker_index].deleting_entry_index_list;
}

bool storage_db_shard_new_is_needed(
        storage_db_shard_t *shard,
        size_t chunk_length) {
    return shard->offset + chunk_length > shard->size;
}

bool storage_db_chunk_data_pre_allocate(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info,
        size_t chunk_length) {
    chunk_info->chunk_length = chunk_length;

    if (db->config->backend_type == STORAGE_DB_BACKEND_TYPE_MEMORY) {
        chunk_info->memory.chunk_data = xalloc_alloc(chunk_length);
        if (!chunk_info->memory.chunk_data) {
            LOG_E(
                    TAG,
                    "Unable to allocate a chunk in memory");

            return false;
        }
    } else {
        storage_db_shard_t *shard;

        if ((shard = storage_db_worker_active_shard(db)) != NULL) {
            if (storage_db_shard_new_is_needed(shard, chunk_length)) {
                LOG_V(
                        TAG,
                        "Shard for worker <%u> full, need to allocate a new one",
                        worker_context_get()->worker_index);
                shard = NULL;
            }
        } else {
            LOG_V(
                    TAG,
                    "No shard allocated for worker <%u> full, need to allocate a new one",
                    worker_context_get()->worker_index);
        }

        if (!shard) {
            LOG_V(
                    TAG,
                    "Allocating a new shard <%lumb> for worker <%u>",
                    db->config->backend.file.shard_size_mb,
                    worker_context_get()->worker_index);

            if (!(shard = storage_db_new_active_shard_per_current_worker(db))) {
                LOG_E(
                        TAG,
                        "Unable to allocate a new shard for worker <%u>",
                        worker_context_get()->worker_index);
                return false;
            }
        }

        chunk_info->file.shard_storage_channel = shard->storage_channel;
        chunk_info->file.chunk_offset = shard->offset;

        shard->offset += chunk_length;
    }

    return true;
}

void storage_db_chunk_data_free(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info) {
    if (chunk_info == NULL) {
        return;
    }

    if (db->config->backend_type == STORAGE_DB_BACKEND_TYPE_MEMORY) {
        xalloc_free(chunk_info->memory.chunk_data);
    } else {
        // TODO: currently not implemented, the data on the disk should be collected by a garbage collector
    }
}

storage_db_shard_t* storage_db_shard_new(
        storage_db_shard_index_t index,
        char *path,
        uint32_t shard_size_mb) {
    storage_channel_t *storage_channel = storage_db_shard_open_or_create_file(
            path,
            true);

    if (!storage_channel) {
        LOG_E(
                TAG,
                "Unable to open or create the new shard <%s> on disk",
                path);
        return NULL;
    }

    if (!storage_db_shard_ensure_size_pre_allocated(storage_channel, shard_size_mb)) {
        LOG_E(
                TAG,
                "Unable to preallocate <%umb> for the shard <%s>",
                shard_size_mb,
                path);
        storage_close(storage_channel);
        return NULL;
    }

    storage_db_shard_t *shard = xalloc_alloc_zero(sizeof(storage_db_shard_t));

    shard->storage_channel = storage_channel;
    shard->index = index;
    shard->offset = 0;
    shard->size = shard_size_mb * 1024 * 1024;
    shard->path = path;
    shard->version = STORAGE_DB_SHARD_VERSION;
    clock_monotonic(&shard->creation_time);

    return shard;
}

bool storage_db_open(
        storage_db_t *db) {
    // TODO: leaving this as placeholder for when the support to load the time series database from the disk will be
    //       added
    return true;
}

storage_db_shard_t *storage_db_new_active_shard(
        storage_db_t *db,
        uint32_t worker_index) {

    // The storage_db_shard_new might trigger a fiber context switch, if it's the case another request, trying to do a
    // write operation in the same thread will also try to acquire the spinlock causing the execution to get stuck as
    // the initial owner of the lock will never get the chance to finish the operation.
    // For this reason, if the lock is in use, the fiber yields to ensure that at some point the original fiber that
    // acquired the lock will get the chance to complete the operations.
    while (!spinlock_try_lock(&db->shards.write_spinlock)) {
        fiber_scheduler_switch_back();
    }

    storage_db_shard_t *shard = storage_db_shard_new(
            db->shards.new_index,
            storage_db_shard_build_path(db->config->backend.file.basedir_path, db->shards.new_index),
            db->config->backend.file.shard_size_mb);

    if (shard) {
        db->shards.new_index++;
        db->workers[worker_index].active_shard = shard;

        double_linked_list_item_t *item = double_linked_list_item_init();
        item->data = shard;
        double_linked_list_push_item(db->shards.opened_shards, item);
    }

    spinlock_unlock(&db->shards.write_spinlock);

    return shard;
}

void storage_db_shard_free(
        storage_db_t *db,
        storage_db_shard_t *shard) {
    // TODO: should also ensure that all the in-flight data being written are actually written
    // TODO: should close the shards properly writing the footer to be able to carry out a quick read
    storage_close(shard->storage_channel);
    xalloc_free(shard);
}

storage_db_shard_t *storage_db_new_active_shard_per_current_worker(
        storage_db_t *db) {
    worker_context_t *worker_context = worker_context_get();
    uint32_t worker_index = worker_context->worker_index;

    return storage_db_new_active_shard(db, worker_index);
}

bool storage_db_close(
    storage_db_t *db) {
    if (db->config->backend_type != STORAGE_DB_BACKEND_TYPE_MEMORY) {
        double_linked_list_item_t* item = NULL;
        while((item = db->shards.opened_shards->head) != NULL) {
            storage_db_shard_free(db, (storage_db_shard_t*)item->data);
            double_linked_list_remove_item(db->shards.opened_shards, item);
            double_linked_list_item_free(item);
        }
    }

    return true;
}

void storage_db_deleted_entry_ring_buffer_per_worker_free(
        storage_db_t *db,
        uint32_t worker_index) {
    storage_db_entry_index_t *entry_index = NULL;
    ring_bounded_queue_spsc_voidptr_t *rb = db->workers[worker_index].deleted_entry_index_ring_buffer;

    if (!rb) {
        return;
    }

    while((entry_index = ring_bounded_queue_spsc_voidptr_dequeue(rb)) != NULL) {
        storage_db_entry_index_free(db, entry_index);
    }

    ring_bounded_queue_spsc_voidptr_free(rb);
}

void storage_db_deleting_entry_index_list_per_worker_free(
        storage_db_t *db,
        uint32_t worker_index) {
    double_linked_list_t *dblist;
    double_linked_list_item_t *item;

    dblist = db->workers[worker_index].deleting_entry_index_list;
    if (!dblist) {
        return;
    }

    while((item = double_linked_list_pop_item(dblist)) != NULL) {
        storage_db_entry_index_t *entry_index = item->data;

        double_linked_list_item_free(item);
        storage_db_entry_index_free(db, entry_index);
    }

    double_linked_list_free(db->workers[worker_index].deleting_entry_index_list);
}

void storage_db_free(
        storage_db_t *db,
        uint32_t workers_count) {
    // Free up the per_worker allocated memory
    for(uint32_t worker_index = 0; worker_index < workers_count; worker_index++) {
        storage_db_deleted_entry_ring_buffer_per_worker_free(db, worker_index);
        storage_db_deleting_entry_index_list_per_worker_free(db, worker_index);
    }

    // Free up the opened_shards lists (the actual cleanup of the shards is done in storage_db_close, here only the
    // memory gets freed up)
    if (db->shards.opened_shards) {
        double_linked_list_free(db->shards.opened_shards);
    }

    // Iterates over the hashtable to free up the entry index
    hashtable_bucket_index_t bucket_index = 0;
    while((bucket_index = hashtable_mcmp_op_iter_all_databases(db->hashtable, bucket_index)) != HASHTABLE_OP_ITER_END) {
        // This is the ONLY place where the hashtable internal data structure can be accessed directly and without
        // transactions as the system is shutting down, the memory is getting freed and there is no concurrency to
        // handle
        storage_db_entry_index_free(
                db,
                (storage_db_entry_index_t *) db->hashtable->ht_current->keys_values[bucket_index].data);
        bucket_index++;
    }

    slots_bitmap_mpmc_free(db->counters_slots_bitmap);
    hashtable_mcmp_free(db->hashtable);
    storage_db_config_free(db->config);
    xalloc_free(db->workers);
    xalloc_free(db);
}

storage_db_entry_index_t *storage_db_entry_index_ring_buffer_new(
        storage_db_t *db) {
    storage_db_entry_index_t *entry_index;
    // The storage_db_entry_indexes are stored into the hashtable and used as reference to keep track of the
    // key and chunks of the value mapped to the index.
    // When the entry on the hashtable is updated, the existing one can't free right away even if the readers_counter
    // is set to zero because potentially one of the workers running on a different core might just have
    // fetched the pointer trying to access it.
    // To avoid a potential issue all the storage_db_entry_index are fetched from a ring buffer which is large.
    // The ring buffer is used in a bit of an odd way, the entries are fetched from it only if the underlying
    // circular queue is full, because this will guarantee that enough time will have passed for any context
    // switch happened in between reading the value from the hashtable and checking if the value is still viable.
    // The size of the ring buffer (so when it's full) is dictated by having enough room to allow any CPU that
    // might be trying to access the value stored in the hashtable to be checked.
    // if the queue is not full then a new entry_index is allocated

    ring_bounded_queue_spsc_voidptr_t *rb = storage_db_worker_deleted_entry_index_ring_buffer(db);

    if (ring_bounded_queue_spsc_voidptr_is_full(rb)) {
        entry_index = ring_bounded_queue_spsc_voidptr_dequeue(rb);
        entry_index->status._cas_wrapper = 0;
        entry_index->snapshot_time_ms = 0;
    } else {
        entry_index = storage_db_entry_index_new();
    }

    entry_index->created_time_ms = clock_monotonic_int64_ms();

    return entry_index;
}

void storage_db_entry_index_touch(
        storage_db_entry_index_t *entry_index) {
    entry_index->last_access_time_ms = clock_monotonic_int64_ms();
    MEMORY_FENCE_STORE();
}

void storage_db_entry_index_ring_buffer_free(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index) {
    ring_bounded_queue_spsc_voidptr_t *rb = storage_db_worker_deleted_entry_index_ring_buffer(db);

    if (ring_bounded_queue_spsc_voidptr_is_full(rb)) {
        storage_db_entry_index_t *entry_index_to_free = ring_bounded_queue_spsc_voidptr_dequeue(rb);
        storage_db_entry_index_free(db, entry_index_to_free);
    }

    bool result = ring_bounded_queue_spsc_voidptr_enqueue(rb, entry_index);
    assert(result);
}

storage_db_entry_index_t *storage_db_entry_index_new() {
    return xalloc_alloc_zero(sizeof(storage_db_entry_index_t));
}

size_t storage_db_chunk_sequence_calculate_chunk_count(
        size_t size) {
    return ceil((double)size / (double)STORAGE_DB_CHUNK_MAX_SIZE);
}

bool storage_db_chunk_sequence_allocate(
        storage_db_t *db,
        storage_db_chunk_sequence_t *chunk_sequence,
        size_t size) {
    bool return_result = false;
    storage_db_chunk_index_t allocated_chunks_count = 0;
    uint32_t chunk_count = storage_db_chunk_sequence_calculate_chunk_count(size);
    size_t remaining_length = size;

    chunk_sequence->size = size;
    chunk_sequence->count = chunk_count;

    if (likely(size > 0)) {
        chunk_sequence->sequence = xalloc_alloc(sizeof(storage_db_chunk_info_t) * chunk_count);

        if (unlikely(!chunk_sequence->sequence)) {
            goto end;
        }

        for(storage_db_chunk_index_t chunk_index = 0; chunk_index < chunk_sequence->count; chunk_index++) {
            storage_db_chunk_info_t *chunk_info = storage_db_chunk_sequence_get(chunk_sequence, chunk_index);

            if (!storage_db_chunk_data_pre_allocate(
                    db,
                    chunk_info,
                    MIN(remaining_length, STORAGE_DB_CHUNK_MAX_SIZE))) {
                goto end;
            }

            remaining_length -= STORAGE_DB_CHUNK_MAX_SIZE;
            allocated_chunks_count++;
        }
    } else {
        chunk_sequence->sequence = NULL;
    }

    return_result = true;

end:
    if (unlikely(!return_result)) {
        if (chunk_sequence->sequence) {
            for(storage_db_chunk_index_t chunk_index = 0; chunk_index < allocated_chunks_count; chunk_index++) {
                storage_db_chunk_data_free(db, storage_db_chunk_sequence_get(chunk_sequence, chunk_index));
            }

            xalloc_free(chunk_sequence->sequence);
            chunk_sequence->sequence = NULL;
            chunk_sequence->size = 0;
            chunk_sequence->count = 0;
        }
    }

    return return_result;
}

void storage_db_chunk_sequence_free_chunks(
        storage_db_t *db,
        storage_db_chunk_sequence_t *sequence) {
    for (
            storage_db_chunk_index_t chunk_index = 0;
            chunk_index < sequence->count;
            chunk_index++) {
        storage_db_chunk_info_t *chunk_info = storage_db_chunk_sequence_get(sequence, chunk_index);
        storage_db_chunk_data_free(db, chunk_info);
    }

    xalloc_free(sequence->sequence);
    sequence->count = 0;
    sequence->sequence = NULL;
    sequence->size = 0;
}

void storage_db_entry_index_chunks_free(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index) {
    if (entry_index->key.size > 0) {
        // If the backend is only memory, the key is managed by the hashtable and the chunks are not stored
        // in memory, so it's necessary to free only the chunks of the values
        if (db->config->backend_type != STORAGE_DB_BACKEND_TYPE_MEMORY) {
            storage_db_chunk_sequence_free_chunks(db, &entry_index->key);
        }
    }

    if (entry_index->value.size > 0) {
        storage_db_chunk_sequence_free_chunks(db, &entry_index->value);
    }
}

void storage_db_entry_index_free(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index) {
    storage_db_entry_index_chunks_free(db, entry_index);
    xalloc_free(entry_index);
}

bool storage_db_entry_chunk_can_read_from_memory(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info) {
    if (db->config->backend_type == STORAGE_DB_BACKEND_TYPE_MEMORY) {
        return true;
    }

    // TODO: with the storage backend the data can be read from the cache only if already available there
    return false;
}

char* storage_db_entry_chunk_read_fast_from_memory(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info) {
    if (db->config->backend_type == STORAGE_DB_BACKEND_TYPE_MEMORY) {
        return chunk_info->memory.chunk_data;
    }

    // This should never happen so if it does it is better to catch it
    assert(false);
    return NULL;
}

bool storage_db_chunk_read(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info,
        char *buffer,
        off_t offset,
        size_t length) {
    assert(offset + length <= chunk_info->chunk_length);

    if (db->config->backend_type == STORAGE_DB_BACKEND_TYPE_MEMORY) {
        if (!memcpy(buffer, chunk_info->memory.chunk_data + offset, length)) {
            return false;
        }
    } else {
        storage_channel_t *channel = chunk_info->file.shard_storage_channel;

        if (!storage_read(
                channel,
                buffer,
                length,
                chunk_info->file.chunk_offset + offset)) {
            LOG_E(
                    TAG,
                    "Failed to read chunk with offset <%ld> long <%lu> bytes (path <%s>)",
                    chunk_info->file.chunk_offset + offset,
                    length,
                    channel->path);

            return false;
        }
    }

    return true;
}

bool storage_db_chunk_write(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info,
        off_t chunk_offset,
        char *buffer,
        size_t buffer_length) {
    if (db->config->backend_type == STORAGE_DB_BACKEND_TYPE_MEMORY) {
        if (!memcpy(chunk_info->memory.chunk_data + chunk_offset, buffer, buffer_length)) {
            return false;
        }
    } else {
        storage_channel_t *channel = chunk_info->file.shard_storage_channel;

        if (!storage_write(
                channel,
                buffer,
                buffer_length,
                chunk_info->file.chunk_offset + chunk_offset)) {
            LOG_E(
                    TAG,
                    "Failed to write chunk with offset <%u> long <%u> bytes (path <%s>)",
                    chunk_info->file.chunk_offset,
                    chunk_info->chunk_length,
                    channel->path);

            return false;
        }
    }

    return true;
}

storage_db_chunk_info_t *storage_db_chunk_sequence_get(
        storage_db_chunk_sequence_t *chunk_sequence,
        storage_db_chunk_index_t chunk_index) {
    if (unlikely(chunk_sequence == NULL || chunk_index >= chunk_sequence->count)) {
        return NULL;
    }

    return chunk_sequence->sequence + chunk_index;
}

char *storage_db_get_chunk_data(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info,
        bool *allocated_new_buffer) {
    char *buffer = NULL;
    *allocated_new_buffer = false;
    if (likely(storage_db_entry_chunk_can_read_from_memory(
            db,
            chunk_info))) {
        buffer = storage_db_entry_chunk_read_fast_from_memory(
                db,
                chunk_info);
    } else {
        *allocated_new_buffer = true;
        buffer = xalloc_alloc(chunk_info->chunk_length);

        if (unlikely(!storage_db_chunk_read(
                db,
                chunk_info,
                buffer,
                0,
                chunk_info->chunk_length))) {
            return NULL;
        }
    }

    return buffer;
}

void storage_db_entry_index_status_increase_readers_counter(
        storage_db_entry_index_t* entry_index,
        storage_db_entry_index_status_t *old_status) {
    storage_db_entry_index_status_t old_status_internal;
    storage_db_entry_index_status_t new_status_internal;

    // Use a CAS loop to increase the readers counters and the access counters
    do {
        MEMORY_FENCE_LOAD();
        old_status_internal._cas_wrapper = entry_index->status._cas_wrapper;
        new_status_internal._cas_wrapper = old_status_internal._cas_wrapper;

        new_status_internal.readers_counter++;
        new_status_internal.accesses_counter++;

        // Keep the max access counter below UINT32_MAX to avoid a rollover
        if (new_status_internal.accesses_counter == UINT32_MAX) {
            new_status_internal.accesses_counter--;
        }

    } while (!__sync_bool_compare_and_swap(
            &entry_index->status._cas_wrapper,
            old_status_internal._cas_wrapper,
            new_status_internal._cas_wrapper));

    // The MSB bit of _cas_wrapper is used for the deleted flag, if the readers_counter gets to 0x7FFFFFFF another lock
    // request would implicitly set the "deleted" flag to true.
    // Although this scenario would cause corruption it's not something we need to solve, it would mean that there are
    // +2 billion clients trying to access the same key which is not really possible with the current hardware
    // available or the current software implementation.
    // A way to solve the issue though is to have enough padding which would allow the counter to be decreased without
    // risking that other worker threads would increase it further causing the overflow.
    // In general just keeping the second MSB "free" for that scenario, the amount of padding required depends on the
    // amount of hardware threads that the cpu(s) are able to run in parallel.
    assert((old_status_internal._cas_wrapper & 0x7FFFFFFF) != 0x7FFFFFFF);

    // If the entry is marked as deleted reduce the readers counter to drop the lock
    if (unlikely(old_status_internal.deleted)) {
        new_status_internal._cas_wrapper = __sync_fetch_and_sub(
                &entry_index->status._cas_wrapper,
                (uint32_t)1);
    }

    if (likely(old_status)) {
        old_status->_cas_wrapper = new_status_internal._cas_wrapper;
    }
}

void storage_db_entry_index_status_decrease_readers_counter(
        storage_db_entry_index_t* entry_index,
        storage_db_entry_index_status_t *old_status) {
    uint32_t old_cas_wrapper_ret = __sync_fetch_and_sub(
            &entry_index->status._cas_wrapper,
            (uint32_t)1);

    // If the previous value of old_cas_wrapper_ret & 0x7FFFFFFF is zero the previous operation caused a negative
    // overflow which set the MSB bit used for "deleted" to 1 triggering corruption, can't really happen unless there
    // is a bug in the reader lock management
    assert((old_cas_wrapper_ret & 0x7FFFFFFF) > 0);

    if (unlikely(old_status)) {
        old_status->_cas_wrapper = old_cas_wrapper_ret;
    }
}

void storage_db_entry_index_status_set_deleted(
        storage_db_entry_index_t* entry_index,
        bool deleted,
        storage_db_entry_index_status_t *old_status) {
    uint32_t cas_wrapper_ret = __sync_fetch_and_or(
            &entry_index->status._cas_wrapper,
            deleted ? 0x80000000 : 0);

    if (likely(old_status != NULL)) {
        old_status->_cas_wrapper = cas_wrapper_ret;
    }
}

void storage_db_worker_garbage_collect_deleting_entry_index_when_no_readers(
        storage_db_t *db) {
    double_linked_list_item_t *item = NULL, *item_next = NULL;
    double_linked_list_t *list = storage_db_worker_deleting_entry_index_list(db);

    // Can't use double_linked_list_iter_next because the code might remove from the list the current item and
    // double_linked_list_iter_next needs access to it
    item_next = list->head;
    uint64_t counter = 0;
    uint64_t max_iterations = 1000 + (list->count > 1000 ? list->count / 25 : 0);
    while((item = item_next) != NULL && counter++ < max_iterations) {
        item_next = item->next;
        storage_db_entry_index_t *entry_index = item->data;

        // No need of an atomic load or a memory barrier, if this code will read an outdated readers_counter and leave
        // the item in the list it will simply reprocess it the next iteration.
        // It is, however, unlikely that an outdated value will be read because of the memory fences and atomic ops
        // everywhere in the code base.
        if (entry_index->status.readers_counter == 0) {
            // Remove the item from the double linked list
            double_linked_list_remove_item(list, item);
            double_linked_list_item_free(item);

            // Free the memory
            storage_db_entry_index_chunks_free(db, entry_index);

            // Add the item to the ring buffer
            storage_db_entry_index_ring_buffer_free(db, entry_index);
        }
    }
}

void storage_db_worker_mark_deleted_or_deleting_previous_entry_index(
        storage_db_t *db,
        storage_db_entry_index_t *previous_entry_index) {
    // hashtable_mcmp_op_set and hashtable_mcmp_op_delete use a lock so there will never be a case with the current
    // implementation where to different invocations of these 2 commands will be returning the same
    // previous_entry_index pointer therefore it's safe to assume that the current thread is the one that is
    // going to do the delete operation moving the entry_index into the deleting list or the deleted ring buffer.
    storage_db_entry_index_status_t old_status;
    storage_db_entry_index_status_set_deleted(
            previous_entry_index,
            true,
            &old_status);

    // if readers counter is set to zero, the entry_index can be enqueued to the ring buffer, for future use, but
    // if there are readers, the entry index can't be freed or reused until readers_counter gets down to zero

    if (old_status.readers_counter == 0) {
        storage_db_entry_index_chunks_free(db, previous_entry_index);
        storage_db_entry_index_ring_buffer_free(db, previous_entry_index);
    } else {
        double_linked_list_item_t *item = double_linked_list_item_init();
        item->data = previous_entry_index;
        double_linked_list_push_item(
                storage_db_worker_deleting_entry_index_list(db),
                item);

        // During development, this list should never get larger than 1000 items, if it does, there is a bug
        assert(storage_db_worker_deleting_entry_index_list(db)->count < 1000);
    }
}

storage_db_entry_index_t *storage_db_get_entry_index(
        storage_db_t *db,
        storage_db_database_number_t database_number,
        transaction_t *transaction,
        char *key,
        size_t key_length) {
    storage_db_entry_index_t *entry_index = NULL;
    hashtable_value_data_t memptr = 0;

    bool res = hashtable_mcmp_op_get(
            db->hashtable,
            database_number,
            transaction,
            key,
            key_length,
            &memptr);

    if (!res) {
        return NULL;
    }

    entry_index = (storage_db_entry_index_t *)memptr;

    if (entry_index) {
        storage_db_entry_index_touch(entry_index);
    }

    return entry_index;
}

bool storage_db_entry_index_is_expired(
        storage_db_entry_index_t *entry_index) {
    if (entry_index && entry_index->expiry_time_ms > 0) {
        if (unlikely(clock_realtime_coarse_int64_ms() > entry_index->expiry_time_ms)) {
            return true;
        }
    }

    return false;
}

int64_t storage_db_entry_index_ttl_ms(
        storage_db_entry_index_t *entry_index) {
    if (entry_index->expiry_time_ms == STORAGE_DB_ENTRY_NO_EXPIRY) {
        return -1;
    }

    return entry_index->expiry_time_ms - clock_realtime_coarse_int64_ms();
}

storage_db_entry_index_t *storage_db_get_entry_index_for_read_prep_no_expired_eviction(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index) {
    storage_db_entry_index_status_t old_status = {0};

    // Try to acquire a reader lock until it's successful or the entry index has been marked as deleted
    storage_db_entry_index_status_increase_readers_counter(
            entry_index,
            &old_status);

    if (unlikely(old_status.deleted)) {
        entry_index = NULL;
    }

    if (storage_db_entry_index_is_expired(entry_index)) {
        storage_db_entry_index_status_decrease_readers_counter(entry_index, NULL);
        entry_index = NULL;
    }

    return entry_index;
}

storage_db_entry_index_t *storage_db_get_entry_index_for_read_prep(
        storage_db_t *db,
        storage_db_database_number_t database_number,
        transaction_t *transaction,
        char *key,
        size_t key_length,
        storage_db_entry_index_t *entry_index) {
    storage_db_entry_index_status_t old_status = { 0 };

    // Try to acquire a reader lock until it's successful or the entry index has been marked as deleted
    storage_db_entry_index_status_increase_readers_counter(
            entry_index,
            &old_status);

    if (unlikely(old_status.deleted)) {
        entry_index = NULL;
    }

    if (storage_db_entry_index_is_expired(entry_index)) {
        storage_db_entry_index_status_decrease_readers_counter(entry_index, NULL);

        // If the storage db entry index is actually expired it's necessary to start a read modify write operation
        // because the check has to be carried out again under the lock to verify if the entry_index has been replaced
        // with a new one and therefore the current entry_index has not to be touched or if the entire bucket has to be
        // freed
        storage_db_op_rmw_status_t rmw_status = { 0 };

        storage_db_entry_index_t *current_entry_index = NULL;
        if (unlikely(!storage_db_op_rmw_begin(
                db,
                transaction,
                database_number,
                key,
                key_length,
                &rmw_status,
                &current_entry_index))) {
            goto end_expired;
        }

        // If the current entry index still matches entry index the storage_db_op_rmw_abort will carry out the clean up
        // for us because it's the same expired entry and the rmw operation is getting aborted. If instead the value
        // has been updated in the meantime, the entry index has to be marked for deletion without touching the
        // hashtable.
        // The current entry index returned by storage_db_op_rmw_begin might be NULL if it's expired (e.g. if it hasn't
        // changed or if the expiration has been set to 1 ms or similar) so the code relies on the current_value field
        // of the rmw operation
        if ((storage_db_entry_index_t*)rmw_status.current_entry_index != entry_index) {
            storage_db_worker_mark_deleted_or_deleting_previous_entry_index(db, entry_index);
            storage_db_op_rmw_abort(db, &rmw_status);
        } else {
            storage_db_op_rmw_commit_delete(db, &rmw_status);
        }

end_expired:
        entry_index = NULL;
    }

    return entry_index;
}

storage_db_entry_index_t *storage_db_get_entry_index_for_read(
        storage_db_t *db,
        storage_db_database_number_t database_number,
        transaction_t *transaction,
        char *key,
        size_t key_length) {
    storage_db_entry_index_t *entry_index = NULL;

    entry_index = storage_db_get_entry_index(db, database_number, transaction, key, key_length);

    if (likely(entry_index)) {
        entry_index = storage_db_get_entry_index_for_read_prep(db, database_number, transaction, key, key_length, entry_index);
    }

    return entry_index;
}

bool storage_db_set_entry_index(
        storage_db_t *db,
        storage_db_database_number_t database_number,
        transaction_t *transaction,
        char *key,
        size_t key_length,
        storage_db_entry_index_t *entry_index) {
    bool out_should_free_key = false;
    hashtable_bucket_index_t out_bucket_index = 0;
    storage_db_entry_index_t *previous_entry_index = NULL;

    storage_db_entry_index_touch(entry_index);

    // After the set the entry_index can't be relied upon as another thread can delete it
    size_t value_size = entry_index->value.size;

    assert(entry_index->status.deleted == false);

    bool res = hashtable_mcmp_op_set(
            db->hashtable,
            database_number,
            transaction,
            key,
            key_length,
            (uintptr_t)entry_index,
            (uintptr_t*)&previous_entry_index,
            &out_bucket_index,
            &out_should_free_key);


    if (res) {
        int64_t counter_data_size_delta = (int64_t)value_size;
        if (previous_entry_index != NULL) {
            counter_data_size_delta -= (int64_t)previous_entry_index->value.size;

            if (storage_db_snapshot_is_in_progress(db)) {
                if (
                        storage_db_snapshot_should_entry_index_be_processed_creation_time(db, previous_entry_index) &&
                        storage_db_snapshot_should_entry_index_be_processed_block_not_processed(db, out_bucket_index)) {
                    storage_db_entry_index_t *previous_entry_index_after_prep =
                            storage_db_get_entry_index_for_read_prep_no_expired_eviction(
                                    db,
                                    previous_entry_index);
                    if (previous_entry_index_after_prep) {
                        storage_db_snapshot_queue_entry_index_to_be_deleted(
                                db,
                                out_bucket_index,
                                key,
                                key_length,
                                previous_entry_index_after_prep);
                    }
                }
            }

            storage_db_worker_mark_deleted_or_deleting_previous_entry_index(db, previous_entry_index);
        }

        STORAGE_DB_COUNTERS_UPDATE(db, database_number, {
            counters->keys_count += previous_entry_index ? 0 : 1;
            counters->data_size += counter_data_size_delta;
        });
    }

    STORAGE_DB_COUNTERS_UPDATE(db, database_number, {
        counters->keys_changed++;
        counters->data_changed += (int64_t)value_size;
    });

    if (out_should_free_key) {
        xalloc_free(key);
    }

    return res;
}

bool storage_db_op_set(
        storage_db_t *db,
        storage_db_database_number_t database_number,
        transaction_t *transaction,
        char *key,
        size_t key_length,
        storage_db_entry_index_value_type_t value_type,
        storage_db_chunk_sequence_t *value_chunk_sequence,
        storage_db_expiry_time_ms_t expiry_time_ms) {
    storage_db_entry_index_t *entry_index = NULL;
    bool result_res = false;

    entry_index = storage_db_entry_index_ring_buffer_new(db);

    // Set up the key if necessary
    if (db->config->backend_type != STORAGE_DB_BACKEND_TYPE_MEMORY) {
        if (!storage_db_chunk_sequence_allocate(db, &entry_index->key, key_length)) {
            LOG_E(TAG, "Unable to allocate the chunks for the key");
            goto end;
        }

        // The key is always one single chunk so no need to be smart here
        if (!storage_db_chunk_write(
                db,
                storage_db_chunk_sequence_get(
                        &entry_index->key,
                        0),
                0,
                key,
                key_length)) {
            LOG_E(TAG, "Unable to write an index entry key");
            goto end;
        }
    }

    if (expiry_time_ms == STORAGE_DB_ENTRY_NO_EXPIRY && db->config->enforced_ttl.default_ms != STORAGE_DB_ENTRY_NO_EXPIRY) {
        expiry_time_ms = clock_realtime_coarse_int64_ms() + db->config->enforced_ttl.default_ms;
    } else if (db->config->enforced_ttl.max_ms != STORAGE_DB_ENTRY_NO_EXPIRY) {
        int64_t max_ttl_ms = clock_realtime_coarse_int64_ms() + db->config->enforced_ttl.max_ms;
        if (db->config->enforced_ttl.max_ms != STORAGE_DB_ENTRY_NO_EXPIRY && expiry_time_ms > max_ttl_ms) {
            expiry_time_ms = max_ttl_ms;
        }
    }

    // Fetch a new entry and assign the key and the value as needed
    entry_index->database_number = database_number;
    entry_index->value.size = value_chunk_sequence->size;
    entry_index->value.count = value_chunk_sequence->count;
    entry_index->value.sequence = value_chunk_sequence->sequence;
    entry_index->expiry_time_ms = expiry_time_ms;

    // Try to store the entry index in the database
    if (!storage_db_set_entry_index(
            db,
            database_number,
            transaction,
            key,
            key_length,
            entry_index)) {
        // As the operation failed while getting ownership of the value, it gets set back to null as to let the caller
        // handle the memory free as necessary
        entry_index->value.size = 0;
        entry_index->value.count = 0;
        entry_index->value.sequence = NULL;
        goto end;
    }

    result_res = true;

end:

    if (!result_res) {
        storage_db_entry_index_free(db, entry_index);
    }

    return result_res;
}

bool storage_db_op_rmw_begin(
        storage_db_t *db,
        transaction_t *transaction,
        storage_db_database_number_t database_number,
        char *key,
        size_t key_length,
        storage_db_op_rmw_status_t *rmw_status,
        storage_db_entry_index_t **current_entry_index) {
    assert(transaction->transaction_id.id != TRANSACTION_ID_NOT_ACQUIRED);

    if (unlikely(!hashtable_mcmp_op_rmw_begin(
            db->hashtable,
            transaction,
            &rmw_status->hashtable,
            database_number,
            key,
            key_length,
            (uintptr_t*)current_entry_index))) {
        return false;
    }

    rmw_status->transaction = transaction;
    rmw_status->current_entry_index = *current_entry_index;

    if (
            *current_entry_index &&
            storage_db_entry_index_is_expired((storage_db_entry_index_t *)*current_entry_index)) {
        rmw_status->delete_entry_index_on_abort = true;
        *current_entry_index = NULL;
    }

    return true;
}

storage_db_entry_index_t *storage_db_op_rmw_current_entry_index_prep_for_read(
        storage_db_t *db,
        storage_db_op_rmw_status_t *rmw_status,
        storage_db_entry_index_t *entry_index) {
    if (entry_index && !rmw_status->delete_entry_index_on_abort) {
        storage_db_entry_index_touch(entry_index);
    }

    // Try to acquire a reader lock until it's successful or the entry index has been marked as deleted
    storage_db_entry_index_status_increase_readers_counter(
            entry_index,
            NULL);

    return entry_index;
}

bool storage_db_op_rmw_commit_metadata(
        storage_db_t *db,
        storage_db_op_rmw_status_t *rmw_status) {
    if (rmw_status->current_entry_index && !rmw_status->delete_entry_index_on_abort) {
        storage_db_entry_index_touch(rmw_status->current_entry_index);
    }

    hashtable_mcmp_op_rmw_commit_update(
            &rmw_status->hashtable,
            (uintptr_t)rmw_status->current_entry_index);

    return true;
}

bool storage_db_op_rmw_commit_update(
        storage_db_t *db,
        storage_db_op_rmw_status_t *rmw_status,
        storage_db_entry_index_value_type_t value_type,
        storage_db_chunk_sequence_t *value_chunk_sequence,
        storage_db_expiry_time_ms_t expiry_time_ms) {
    storage_db_entry_index_t *entry_index = NULL;
    bool result_res = false;

    entry_index = storage_db_entry_index_ring_buffer_new(db);

    // Set up the key if necessary
    if (db->config->backend_type != STORAGE_DB_BACKEND_TYPE_MEMORY) {
        if (!storage_db_chunk_sequence_allocate(db, &entry_index->key, rmw_status->hashtable.key_length)) {
            LOG_E(TAG, "Unable to allocate the chunks for the key");
            goto end;
        }

        // The key is always one single chunk so no need to be smart here
        if (!storage_db_chunk_write(
                db,
                storage_db_chunk_sequence_get(
                        &entry_index->key,
                        0),
                0,
                rmw_status->hashtable.key,
                rmw_status->hashtable.key_length)) {
            LOG_E(TAG, "Unable to write an index entry key");
            goto end;
        }
    }

    if (expiry_time_ms == STORAGE_DB_ENTRY_NO_EXPIRY && db->config->enforced_ttl.default_ms != STORAGE_DB_ENTRY_NO_EXPIRY) {
        expiry_time_ms = clock_realtime_coarse_int64_ms() + db->config->enforced_ttl.default_ms;
    } else if (db->config->enforced_ttl.max_ms != STORAGE_DB_ENTRY_NO_EXPIRY) {
        int64_t max_ttl_ms = clock_realtime_coarse_int64_ms() + db->config->enforced_ttl.max_ms;
        if (db->config->enforced_ttl.max_ms != STORAGE_DB_ENTRY_NO_EXPIRY && expiry_time_ms > max_ttl_ms) {
            expiry_time_ms = max_ttl_ms;
        }
    }

    // Fetch a new entry and assign the key and the value as needed
    entry_index->database_number = rmw_status->hashtable.database_number;
    entry_index->value.size = value_chunk_sequence->size;
    entry_index->value.count = value_chunk_sequence->count;
    entry_index->value.sequence = value_chunk_sequence->sequence;
    entry_index->expiry_time_ms = expiry_time_ms;

    storage_db_entry_index_touch(entry_index);

    // After the set the entry_index can't be relied upon as another thread can delete it
    size_t value_size = entry_index->value.size;

    assert(entry_index->status.deleted == false);

    hashtable_mcmp_op_rmw_commit_update(
            &rmw_status->hashtable,
            (uintptr_t)entry_index);

    int64_t counter_data_size_delta = (int64_t)value_size;
    if (rmw_status->hashtable.current_value != 0) {
        counter_data_size_delta -=
                (int64_t)((storage_db_entry_index_t *)rmw_status->hashtable.current_value)->value.size;

        storage_db_worker_mark_deleted_or_deleting_previous_entry_index(
                db,
                (storage_db_entry_index_t *)rmw_status->hashtable.current_value);
    }

    STORAGE_DB_COUNTERS_UPDATE(db, rmw_status->hashtable.database_number, {
        counters->keys_count += rmw_status->hashtable.current_value ? 0 : 1;
        counters->data_size += counter_data_size_delta;
        counters->keys_changed++;
        counters->data_changed += (int64_t)value_size;
    });

    result_res = true;

end:

    if (!result_res) {
        storage_db_entry_index_free(db, entry_index);

        // Abort the underlying rmw operation in the hashtable if the commit fails
        hashtable_mcmp_op_rmw_abort(&rmw_status->hashtable);
    }

    return result_res;
}

void storage_db_op_rmw_commit_rename(
        storage_db_t *db,
        storage_db_op_rmw_status_t *rmw_status_source,
        storage_db_op_rmw_status_t *rmw_status_destination) {
    hashtable_mcmp_op_rmw_commit_update(
            &rmw_status_destination->hashtable,
            (uintptr_t)rmw_status_source->current_entry_index);

    if (rmw_status_destination->hashtable.current_value != 0) {
        storage_db_worker_mark_deleted_or_deleting_previous_entry_index(
                db,
                (storage_db_entry_index_t *)rmw_status_destination->hashtable.current_value);
    }

    hashtable_mcmp_op_rmw_commit_delete(&rmw_status_source->hashtable);

    if (rmw_status_source->current_entry_index && !rmw_status_source->delete_entry_index_on_abort) {
        storage_db_entry_index_touch(rmw_status_source->current_entry_index);
    }
}

void storage_db_op_rmw_commit_delete(
        storage_db_t *db,
        storage_db_op_rmw_status_t *rmw_status) {
    STORAGE_DB_COUNTERS_UPDATE(db, rmw_status->hashtable.database_number, {
        counters->keys_count--;
        counters->data_size -= (int64_t) ((storage_db_entry_index_t *) rmw_status->hashtable.current_value)->value.size;
        counters->keys_changed++;
        counters->data_changed += (int64_t) ((storage_db_entry_index_t *) rmw_status->hashtable.current_value)->value.size;
    });

    storage_db_worker_mark_deleted_or_deleting_previous_entry_index(
            db,
            (storage_db_entry_index_t *)rmw_status->hashtable.current_value);

    hashtable_mcmp_op_rmw_commit_delete(&rmw_status->hashtable);
}

void storage_db_op_rmw_abort(
        storage_db_t *db,
        storage_db_op_rmw_status_t *rmw_status) {
    if (rmw_status->delete_entry_index_on_abort) {
        storage_db_op_rmw_commit_delete(db, rmw_status);
    } else {
        hashtable_mcmp_op_rmw_abort(&rmw_status->hashtable);
    }
}

bool storage_db_op_delete(
        storage_db_t *db,
        storage_db_database_number_t database_number,
        transaction_t *transaction,
        char *key,
        size_t key_length) {
    storage_db_entry_index_t *current_entry_index = NULL;

    bool res = hashtable_mcmp_op_delete(
            db->hashtable,
            database_number,
            transaction,
            key,
            key_length,
            (uintptr_t*)&current_entry_index);

    if (res && current_entry_index != NULL) {
        STORAGE_DB_COUNTERS_UPDATE(db, database_number, {
            counters->keys_count--;
            counters->data_size -= (int64_t) current_entry_index->value.size;
            counters->keys_changed++;
            counters->data_changed += (int64_t) current_entry_index->value.size;
        });

        storage_db_worker_mark_deleted_or_deleting_previous_entry_index(db, current_entry_index);
    }

    return res;
}

bool storage_db_op_delete_by_index(
        storage_db_t *db,
        storage_db_database_number_t database_number,
        transaction_t *transaction,
        bool already_locked_for_read,
        hashtable_bucket_index_t bucket_index) {
    storage_db_entry_index_t *current_entry_index = NULL;

    bool res = hashtable_mcmp_op_delete_by_index(
            db->hashtable,
            database_number,
            transaction,
            already_locked_for_read,
            bucket_index,
            (uintptr_t*)&current_entry_index);

    if (res && current_entry_index != NULL) {
        STORAGE_DB_COUNTERS_UPDATE(db, database_number, {
            counters->keys_count--;
            counters->data_size -= (int64_t)current_entry_index->value.size;
            counters->keys_changed++;
            counters->data_changed += (int64_t)current_entry_index->value.size;
        });

        storage_db_worker_mark_deleted_or_deleting_previous_entry_index(db, current_entry_index);
    }

    return res;
}

bool storage_db_op_delete_by_index_all_databases(
        storage_db_t *db,
        transaction_t *transaction,
        bool already_locked_for_read,
        hashtable_bucket_index_t bucket_index,
        storage_db_entry_index_t **out_current_entry_index) {
    storage_db_entry_index_t *current_entry_index = NULL;

    bool res = hashtable_mcmp_op_delete_by_index_all_databases(
            db->hashtable,
            transaction,
            already_locked_for_read,
            bucket_index,
            (uintptr_t*)&current_entry_index);

    if (likely(res && current_entry_index != NULL)) {
        STORAGE_DB_COUNTERS_UPDATE(db, current_entry_index->database_number, {
            counters->keys_count--;
            counters->data_size -= (int64_t)current_entry_index->value.size;
            counters->keys_changed++;
            counters->data_changed += (int64_t)current_entry_index->value.size;
        });

        *out_current_entry_index = current_entry_index;
        storage_db_worker_mark_deleted_or_deleting_previous_entry_index(db, current_entry_index);
    } else {
        *out_current_entry_index = NULL;
    }

    return res;
}

int64_t storage_db_op_get_keys_count_per_db(
        storage_db_t *db,
        storage_db_database_number_t database_number) {
    storage_db_counters_t counters = { 0 };
    storage_db_counters_sum_per_db(db, database_number, &counters);

    return counters.keys_count;
}

int64_t storage_db_op_get_data_size_per_db(
        storage_db_t *db,
        storage_db_database_number_t database_number) {
    storage_db_counters_t counters = { 0 };
    storage_db_counters_sum_per_db(db, database_number, &counters);

    return counters.data_size;
}

int64_t storage_db_op_get_keys_count_global(
        storage_db_t *db) {
    storage_db_counters_t counters = { 0 };
    storage_db_counters_sum_global(db, &counters);

    return counters.keys_count;
}

int64_t storage_db_op_get_data_size_global(
        storage_db_t *db) {
    storage_db_counters_t counters = { 0 };
    storage_db_counters_sum_global(db, &counters);

    return counters.data_size;
}

char *storage_db_op_random_key(
        storage_db_t *db,
        storage_db_database_number_t database_number,
        transaction_t *transaction,
        hashtable_key_length_t *key_size) {
    char *key = NULL;

    while(storage_db_op_get_keys_count_per_db(db, database_number) > 0 &&
          !hashtable_mcmp_op_get_random_key_try(
                  db->hashtable,
                  database_number,
                  transaction,
                  &key,
                  key_size)) {
        // do nothing
    }

    return key;
}

bool storage_db_op_flush_sync(
        storage_db_t *db,
        storage_db_database_number_t database_number) {
    // As the resizing has to be taken into account but not yet implemented, the assert will catch if the resizing is
    // implemented without having dealt with the flush
    assert(db->hashtable->ht_old == NULL);
    int64_t deletion_start_ms = clock_monotonic_int64_ms();

    // Iterates over the hashtable to free up the entry index
    hashtable_bucket_index_t bucket_index = 0;
    while((bucket_index = hashtable_mcmp_op_iter(
            db->hashtable,
            database_number,
            bucket_index)) != HASHTABLE_OP_ITER_END) {
        // To avoid long-running transactions, a transaction is created and destroyed for each entry
        transaction_t transaction = {0};
        transaction_acquire(&transaction);

        storage_db_entry_index_t *entry_index = NULL;
        storage_db_database_number_t entry_database_number;
        if (hashtable_mcmp_op_get_by_index(
                db->hashtable,
                &transaction,
                bucket_index,
                &entry_database_number,
                (void *) &entry_index)) {
            // Ensure that the entry is from the correct database and that it's older than the deletion start time, to
            // avoid deleting entries that have been created after the flush has started
            if (database_number == entry_database_number && entry_index->created_time_ms <= deletion_start_ms) {
                storage_db_op_delete_by_index(
                        db,
                        database_number,
                        &transaction,
                        true,
                        bucket_index);
            }
        }

        transaction_release(&transaction);
        bucket_index++;
    }

    return true;
}

storage_db_key_and_key_length_t *storage_db_op_get_keys(
        storage_db_t *db,
        storage_db_database_number_t database_number,
        uint64_t cursor,
        uint64_t count,
        char *pattern,
        size_t pattern_length,
        uint64_t *keys_count,
        uint64_t *cursor_next) {
    hashtable_key_data_t *key;
    hashtable_key_length_t key_size;
    bool end_reached = false;
    *keys_count = 0;
    *cursor_next = cursor;

    if (unlikely(storage_db_op_get_keys_count_per_db(db, database_number)) == 0) {
        return NULL;
    }

    if (cursor >= db->hashtable->ht_current->buckets_count_real) {
        return NULL;
    }

    if (count == 0) {
        count = db->hashtable->ht_current->buckets_count_real;
    }

    uint64_t keys_allocated_count = 8;
    storage_db_key_and_key_length_t *keys =
            xalloc_alloc(sizeof(storage_db_key_and_key_length_t) * keys_allocated_count);

    // As the resizing has to be taken into account but not yet implemented, the assert will catch if the resizing is
    // implemented without having dealt with the flush
    assert(db->hashtable->ht_old == NULL);
    int64_t scan_start_ms = clock_monotonic_int64_ms();

    // Iterates over the hashtable to free up the entry index
    do {
        hashtable_bucket_index_t bucket_index = hashtable_mcmp_op_iter(
                db->hashtable,
                database_number,
                *cursor_next);

        if (unlikely(bucket_index == HASHTABLE_OP_ITER_END)) {
            end_reached = true;
            break;
        }

        *cursor_next = bucket_index + 1;

        // Fetch the entry index
        transaction_t transaction_read = {0};
        transaction_acquire(&transaction_read);

        storage_db_entry_index_t *entry_index = NULL;
        if (!hashtable_mcmp_op_get_by_index_all_databases(
                db->hashtable,
                &transaction_read,
                bucket_index,
                (void*)&entry_index)) {
            transaction_release(&transaction_read);
            continue;
        }

        if (unlikely(entry_index->created_time_ms > scan_start_ms)) {
            transaction_release(&transaction_read);
            continue;
        }

        if (unlikely(!hashtable_mcmp_op_get_key(
                db->hashtable,
                database_number,
                &transaction_read,
                bucket_index,
                &key,
                &key_size))) {
            transaction_release(&transaction_read);
            continue;
        }

        transaction_release(&transaction_read);

        if (likely(pattern_length > 0)) {
            if (!utils_string_glob_match(key, key_size, pattern, pattern_length)) {
                xalloc_free(key);
                continue;
            }
        }

        if (unlikely(*keys_count == keys_allocated_count)) {
            keys_allocated_count *= 2;
            keys = xalloc_realloc(keys, sizeof(storage_db_key_and_key_length_t) * keys_allocated_count);
        }

        keys[*keys_count].key = key;
        keys[*keys_count].key_size = key_size;
        (*keys_count)++;
    } while(likely(*cursor_next < cursor + count));

    if (unlikely(end_reached)) {
        *cursor_next = 0;
    }

    return keys;
}

void storage_db_free_key_and_key_length_list(
        storage_db_key_and_key_length_t *keys,
        uint64_t keys_count) {
    for(uint64_t index = 0; index < keys_count; index++) {
        xalloc_free(keys[index].key);
    }
    xalloc_free(keys);
}

static inline void storage_db_keys_eviction_bitonic_sort_16_elements_exchange(
        uint128_t *kv,
        int i,
        int j) {
    uint128_t t = kv[i];
    kv[i] = kv[j];
    kv[j] = t;
}

void storage_db_keys_eviction_bitonic_sort_16_elements(storage_db_keys_eviction_kv_list_entry_t *kv) {
    // Bitonic implementation from https://courses.cs.duke.edu//fall08/cps196.1/Pthreads/bitonic.c optimized for an
    // array of fixed size of 16 elements and unrolled. AVX2 is not used as it doesn't seem to be faster than the
    // non-AVX2 version. Also not using SIMD makes the code more portable allowing to run it on ARM easily.
    // The size of the array is predefined and set to 16 via
    // STORAGE_DB_KEYS_EVICTION_BITONIC_SORT_16_ELEMENTS_ARRAY_LENGTH
    uint16_t i, j, k, array_length = STORAGE_DB_KEYS_EVICTION_BITONIC_SORT_16_ELEMENTS_ARRAY_LENGTH;

    #pragma unroll(4)
    for (k = 2; k <= array_length; k = 2 * k) {

        #pragma unroll(4)
        for (j = k >> 1; j > 0; j= j >> 1) {

            #pragma unroll(16)
            for (i = 0; i < array_length; i++) {
                int ij = i ^ j;

                if (ij > i) {
                    // For better performances, the two checks below, compare only the keys instead of the whole 128bit
                    // value
                    if ((i & k) == 0 && kv[i].key > kv[ij].key) {
                        storage_db_keys_eviction_bitonic_sort_16_elements_exchange((uint128_t*)kv, i,ij);
                    }

                    if ((i & k) != 0 && kv[i].key < kv[ij].key) {
                        storage_db_keys_eviction_bitonic_sort_16_elements_exchange((uint128_t*)kv, i,ij);
                    }
                }
            }
        }
    }
}

uint8_t storage_db_keys_eviction_run_worker(
        storage_db_t *db,
        storage_db_counters_t *counters,
        bool only_ttl,
        config_database_keys_eviction_policy_t policy) {
    assert(STORAGE_DB_KEYS_EVICTION_EVICT_FIRST_N_KEYS <= STORAGE_DB_KEYS_EVICTION_BITONIC_SORT_16_ELEMENTS_ARRAY_LENGTH);

    storage_db_keys_eviction_kv_list_entry_t keys_evitction_candidates_list[
            STORAGE_DB_KEYS_EVICTION_BITONIC_SORT_16_ELEMENTS_ARRAY_LENGTH];
    uint64_t keys_eviction_candidates_list_count;
    uint8_t keys_evicted_count = 0;

    // Calculate the segment of the hashtable that has to be covered by this worker
    uint64_t buckets_end = db->hashtable->ht_current->buckets_count_real;

    // As the resizing has to be taken into account but not yet implemented, the assert will catch if the resizing is
    // implemented without having dealt with the flush
    assert(db->hashtable->ht_old == NULL);

    // Iterates over the hashtable to free up the entry index
    hashtable_bucket_index_t bucket_index = 0;
    hashtable_bucket_count_t segment_size =
            buckets_end / STORAGE_DB_KEYS_EVICTION_BITONIC_SORT_16_ELEMENTS_ARRAY_LENGTH;
    uint32_t search_attempts = 0;
    for(
            keys_eviction_candidates_list_count = 0;
            keys_eviction_candidates_list_count < STORAGE_DB_KEYS_EVICTION_BITONIC_SORT_16_ELEMENTS_ARRAY_LENGTH &&
                search_attempts < STORAGE_DB_KEYS_EVICTION_ITER_MAX_SEARCH_ATTEMPTS;
            keys_eviction_candidates_list_count++) {
        hashtable_bucket_count_t increment = random_generate() % segment_size;
        bucket_index += increment;

        // Tries to fetch the next entry index
        bucket_index = hashtable_mcmp_op_iter_max_distance_all_databases(
                db->hashtable,
                bucket_index,
                STORAGE_DB_KEYS_EVICTION_ITER_MAX_DISTANCE * search_attempts);

        if (unlikely(bucket_index >= buckets_end || bucket_index == HASHTABLE_OP_ITER_END)) {
            // In case data is null but the bucket index is not at the end, it means that the max distance has been
            // reached and no bucket has been found.
            if (unlikely(bucket_index >= buckets_end)) {
                // Restarts from the beginning and retry
                bucket_index = 0;
            }
            keys_eviction_candidates_list_count--;
            search_attempts++;
            continue;
        }

        // Bucket found, reset the search attempts
        search_attempts = 0;

        // Fetch the entry index
        transaction_t transaction_read = {0};
        transaction_acquire(&transaction_read);

        storage_db_entry_index_t *entry_index = NULL;
        if (!hashtable_mcmp_op_get_by_index_all_databases(
                db->hashtable,
                &transaction_read,
                bucket_index,
                (void*)&entry_index)) {
            transaction_release(&transaction_read);

            // Drops the increment and retry
            bucket_index -= increment;
            keys_eviction_candidates_list_count--;
            continue;
        }

        // If only the keys with expiry time have to be evicted and the current key has no expiry time, then skip it
        if (unlikely(only_ttl && entry_index->expiry_time_ms == STORAGE_DB_ENTRY_NO_EXPIRY)) {
            transaction_release(&transaction_read);

            // Drops the increment and retry
            bucket_index -= increment;
            keys_eviction_candidates_list_count--;
            continue;
        }

        // Fetch the sorting key
        uint64_t sort_key;
        switch(policy) {
            case CONFIG_DATABASE_KEYS_EVICTION_POLICY_RANDOM:
                sort_key = random_generate();
                break;
            case CONFIG_DATABASE_KEYS_EVICTION_POLICY_LRU:
                sort_key = entry_index->last_access_time_ms;
                break;
            case CONFIG_DATABASE_KEYS_EVICTION_POLICY_LFU:
                sort_key = entry_index->status.accesses_counter;
                break;
            case CONFIG_DATABASE_KEYS_EVICTION_POLICY_TTL:
                sort_key = entry_index->expiry_time_ms == STORAGE_DB_ENTRY_NO_EXPIRY
                        ? UINT64_MAX
                        : entry_index->expiry_time_ms;
                break;
            default:
                assert(false);
        }

        // Set the sort key and the value (the key of the entry)
        keys_evitction_candidates_list[keys_eviction_candidates_list_count].key = sort_key;
        keys_evitction_candidates_list[keys_eviction_candidates_list_count].value = bucket_index;

        transaction_release(&transaction_read);
    }

    // If too many search attempts have been carried out, fill the rest of the array with key set to UINT64_MAX and
    // value set to UINT64_MAX
    if (unlikely(search_attempts >= STORAGE_DB_KEYS_EVICTION_ITER_MAX_SEARCH_ATTEMPTS)) {
        #pragma unroll(STORAGE_DB_KEYS_EVICTION_BITONIC_SORT_16_ELEMENTS_ARRAY_LENGTH)
        for (;
                keys_eviction_candidates_list_count < STORAGE_DB_KEYS_EVICTION_BITONIC_SORT_16_ELEMENTS_ARRAY_LENGTH;
                keys_eviction_candidates_list_count++) {
            keys_evitction_candidates_list[keys_eviction_candidates_list_count].key = UINT64_MAX;
            keys_evitction_candidates_list[keys_eviction_candidates_list_count].value = UINT64_MAX;
        }
    }

    // Sort the keys
    storage_db_keys_eviction_bitonic_sort_16_elements(keys_evitction_candidates_list);

    // Delete the first 10 keys
    uint8_t evict_first_n_keys = STORAGE_DB_KEYS_EVICTION_EVICT_FIRST_N_KEYS;

    // Starts a new transaction to delete the selected entries
    transaction_t transaction_delete = {0};
    transaction_acquire(&transaction_delete);

#pragma unroll(STORAGE_DB_KEYS_EVICTION_EVICT_FIRST_N_KEYS)
    for (
            uint8_t key_eviction_list_entry_index = 0;
            key_eviction_list_entry_index < evict_first_n_keys &&
                    key_eviction_list_entry_index < STORAGE_DB_KEYS_EVICTION_BITONIC_SORT_16_ELEMENTS_ARRAY_LENGTH;
            key_eviction_list_entry_index++) {
        storage_db_entry_index_t *entry_index = NULL;
        storage_db_keys_eviction_kv_list_entry_t *key_eviction_list_entry =
                &keys_evitction_candidates_list[key_eviction_list_entry_index];

        // Check if the key is set to UINT64_MAX and the value is set to UINT64_MAX, in which case there are no more
        // keys to evict in the list
        if (unlikely(key_eviction_list_entry->key == UINT64_MAX &&
            key_eviction_list_entry->value == UINT64_MAX)) {
            break;
        }

        // First try to read the entry to ensure it's still the same one
        if (!hashtable_mcmp_op_get_by_index_all_databases(
                db->hashtable,
                &transaction_delete,
                bucket_index,
                (void*)&entry_index)) {
            // If the operation fails, increment the amount of keys to evict by one and continue
            evict_first_n_keys++;
            continue;
        }

        // TODO: check that the bucket didn't change in the meantime


        // Delete the key
        if (storage_db_op_delete_by_index_all_databases(
                db,
                &transaction_delete,
                true,
                key_eviction_list_entry->value,
                &entry_index)) {
            keys_evicted_count++;

            counters->keys_count--;
            counters->data_size -= (int64_t)entry_index->value.size;
        } else {
            // Should never fail, but just in case increment the amount of keys to evict by one and continue
            evict_first_n_keys++;
        }
    }

    transaction_release(&transaction_delete);

    return keys_evicted_count;
}

#pragma clang diagnostic pop