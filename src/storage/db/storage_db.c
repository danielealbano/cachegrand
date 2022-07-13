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
#include <math.h>
#include <fcntl.h>
#include <pow2.h>
#include <stdatomic.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"
#include "data_structures/hashtable/mcmp/hashtable_op_get.h"
#include "data_structures/hashtable/mcmp/hashtable_op_set.h"
#include "data_structures/hashtable/mcmp/hashtable_op_delete.h"
#include "slab_allocator.h"
#include "fiber.h"
#include "fiber_scheduler.h"
#include "log/log.h"
#include "config.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/storage.h"

#include "storage_db.h"

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
    path = slab_allocator_mem_alloc(required_length + 1);

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
    return slab_allocator_mem_alloc_zero(sizeof(storage_db_config_t));
}

void storage_db_config_free(
        storage_db_config_t* config) {
    slab_allocator_mem_free(config);
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
    hashtable_config->initial_size = pow2_next(config->max_keys);

    // Initialize the hashtable
    hashtable = hashtable_mcmp_init(hashtable_config);
    if (!hashtable) {
        LOG_E(TAG, "Unable to allocate memory for the hashtable");
        goto fail;
    }

    // Initialize the per-worker set of information
    workers = slab_allocator_mem_alloc_zero(sizeof(storage_db_worker_t) * workers_count);
    if (!workers) {
        LOG_E(TAG, "Unable to allocate memory for the per worker configurations");
        goto fail;
    }

    // Initialize the per worker needed information
    for(uint32_t worker_index = 0; worker_index < workers_count; worker_index++) {
        small_circular_queue_t *deleted_entry_index_ring_buffer =
                small_circular_queue_init(STORAGE_DB_WORKER_ENTRY_INDEX_RING_BUFFER_SIZE);

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
    db = slab_allocator_mem_alloc_zero(sizeof(storage_db_t));
    if (!db) {
        LOG_E(TAG, "Unable to allocate memory for the storage db");
        goto fail;
    }

    // Sets up all the db related information
    db->config = config;
    db->workers = workers;
    db->hashtable = hashtable;

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
                small_circular_queue_free(workers[worker_index].deleted_entry_index_ring_buffer);
            }

            if (workers[worker_index].deleting_entry_index_list) {
                double_linked_list_free(workers[worker_index].deleting_entry_index_list);
            }
        }

        slab_allocator_mem_free(workers);
    }

    if (db) {
        // This can't really happen with the current implementation but better to have it in place to avoid future
        // bugs caused by code refactorings
        if (db->shards.opened_shards) {
            double_linked_list_free(db->shards.opened_shards);
        }

        slab_allocator_mem_free(db);
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

small_circular_queue_t *storage_db_worker_deleted_entry_index_ring_buffer(
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
        chunk_info->memory.chunk_data = slab_allocator_mem_alloc(chunk_length);
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
    if (db->config->backend_type == STORAGE_DB_BACKEND_TYPE_MEMORY) {
        slab_allocator_mem_free(chunk_info->memory.chunk_data);
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

    storage_db_shard_t *shard = slab_allocator_mem_alloc_zero(sizeof(storage_db_shard_t));

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
    while (!spinlock_lock(&db->shards.write_spinlock, false)) {
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
    slab_allocator_mem_free(shard);
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

void storage_db_free(
        storage_db_t *db,
        uint32_t workers_count) {
    for(uint32_t worker_index = 0; worker_index < workers_count; worker_index++) {
        if (db->workers[worker_index].deleted_entry_index_ring_buffer) {
            storage_db_entry_index_t *entry_index = NULL;
            while((entry_index = small_circular_queue_dequeue(
                    db->workers[worker_index].deleted_entry_index_ring_buffer)) != NULL) {
                storage_db_entry_index_free(db, entry_index);
            }

            small_circular_queue_free(db->workers[worker_index].deleted_entry_index_ring_buffer);
        }

        if (db->workers[worker_index].deleting_entry_index_list) {
            double_linked_list_item_t *item = NULL;
            while((item = double_linked_list_pop_item(
                    db->workers[worker_index].deleting_entry_index_list)) != NULL) {
                storage_db_entry_index_t *entry_index = item->data;

                double_linked_list_item_free(item);
                storage_db_entry_index_free(db, entry_index);
            }

            double_linked_list_free(db->workers[worker_index].deleting_entry_index_list);
        }
    }

    if (db->shards.opened_shards) {
        double_linked_list_free(db->shards.opened_shards);
    }

    for(uint64_t bucket_index = 0; bucket_index < db->hashtable->ht_current->buckets_count_real; bucket_index++) {
        hashtable_key_value_volatile_t *key_value = &db->hashtable->ht_current->keys_values[bucket_index];

        if (
                HASHTABLE_KEY_VALUE_IS_EMPTY(key_value->flags) ||
                HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags, HASHTABLE_KEY_VALUE_FLAG_DELETED)) {
            continue;
        }

        if (!HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags, HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE)) {
            slab_allocator_mem_free(key_value->external_key.data);
        }

        storage_db_entry_index_t *data = (storage_db_entry_index_t *)key_value->data;
        storage_db_entry_index_free(db, data);
    }

    hashtable_mcmp_free(db->hashtable);
    storage_db_config_free(db->config);
    slab_allocator_mem_free(db->workers);
    slab_allocator_mem_free(db);
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

    small_circular_queue_t *scb = storage_db_worker_deleted_entry_index_ring_buffer(db);

    if (small_circular_queue_is_full(scb)) {
        entry_index = small_circular_queue_dequeue(scb);
        entry_index->status._cas_wrapper = 0;
    } else {
        entry_index = storage_db_entry_index_new();
    }

    return entry_index;
}

void storage_db_entry_index_ring_buffer_free(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index) {
    small_circular_queue_t *scb = storage_db_worker_deleted_entry_index_ring_buffer(db);

    // If the queue is full, the entry in the head can be dequeued and freed because it means it has lived enough
    if (small_circular_queue_is_full(scb)) {
        storage_db_entry_index_t *entry_index_to_free = small_circular_queue_dequeue(scb);
        storage_db_entry_index_free(db, entry_index_to_free);
    }

    small_circular_queue_enqueue(scb, entry_index);
}

storage_db_entry_index_t *storage_db_entry_index_new() {
    return slab_allocator_mem_alloc_zero(sizeof(storage_db_entry_index_t));
}

bool storage_db_entry_index_allocate_key_chunks(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index,
        size_t key_length) {
    if (db->config->backend_type == STORAGE_DB_BACKEND_TYPE_MEMORY) {
        return true;
    }

    uint32_t chunk_count = ceil((double)key_length / (double)STORAGE_DB_CHUNK_MAX_SIZE);

    entry_index->key_length = key_length;
    entry_index->key_chunks_info = slab_allocator_mem_alloc(sizeof(storage_db_chunk_info_t) * chunk_count);
    entry_index->key_chunks_count = chunk_count;

    size_t remaining_length = key_length;
    for(storage_db_chunk_index_t chunk_index = 0; chunk_index < entry_index->key_chunks_count; chunk_index++) {
        storage_db_chunk_info_t *chunk_info = storage_db_entry_key_chunk_get(entry_index, chunk_index);

        if (!storage_db_chunk_data_pre_allocate(
                db,
                chunk_info,
                min(remaining_length, STORAGE_DB_CHUNK_MAX_SIZE))) {
            slab_allocator_mem_free(entry_index->key_chunks_info);
            return false;
        }

        remaining_length -= STORAGE_DB_CHUNK_MAX_SIZE;
    }

    return true;
}

bool storage_db_entry_index_allocate_value_chunks(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index,
        size_t value_length) {
    uint32_t chunk_count = ceil((double)value_length / (double)STORAGE_DB_CHUNK_MAX_SIZE);

    entry_index->value_length = value_length;
    entry_index->value_chunks_info = slab_allocator_mem_alloc(sizeof(storage_db_chunk_info_t) * chunk_count);
    entry_index->value_chunks_count = chunk_count;

    size_t remaining_length = value_length;
    for(storage_db_chunk_index_t chunk_index = 0; chunk_index < entry_index->value_chunks_count; chunk_index++) {
        storage_db_chunk_info_t *chunk_info = storage_db_entry_value_chunk_get(entry_index, chunk_index);

        if (!storage_db_chunk_data_pre_allocate(
                db,
                chunk_info,
                min(remaining_length, STORAGE_DB_CHUNK_MAX_SIZE))) {
            // TODO: If the operation fails all the allocated values should be freed as this might lead to memory leaks
            slab_allocator_mem_free(entry_index->value_chunks_info);
            return false;
        }

        remaining_length -= STORAGE_DB_CHUNK_MAX_SIZE;
    }

    return true;
}

void storage_db_entry_index_chunks_free(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index) {
    if (entry_index->key_chunks_info) {
        // If the backend is only memory, the key is managed by the hashtable and the chunks are not stored
        // in memory, so it's necessary to free only the chunks of the values
        if (db->config->backend_type != STORAGE_DB_BACKEND_TYPE_MEMORY) {
            for(
                    storage_db_chunk_index_t chunk_index = 0;
                    chunk_index < entry_index->key_chunks_count;
                    chunk_index++) {
                storage_db_chunk_info_t *chunk_info = storage_db_entry_key_chunk_get(entry_index, chunk_index);
                storage_db_chunk_data_free(db, chunk_info);
            }

            slab_allocator_mem_free(entry_index->key_chunks_info);
            entry_index->key_chunks_count = 0;
            entry_index->key_chunks_info = NULL;
        }
    }

    if (entry_index->value_chunks_info) {
        for (
                storage_db_chunk_index_t chunk_index = 0;
                chunk_index < entry_index->value_chunks_count;
                chunk_index++) {
            storage_db_chunk_info_t *chunk_info = storage_db_entry_value_chunk_get(entry_index, chunk_index);
            storage_db_chunk_data_free(db, chunk_info);
        }

        slab_allocator_mem_free(entry_index->value_chunks_info);
        entry_index->value_chunks_count = 0;
        entry_index->value_chunks_info = NULL;
    }
}

void storage_db_entry_index_free(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index) {
    storage_db_entry_index_chunks_free(db, entry_index);

    slab_allocator_mem_free(entry_index);
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

bool storage_db_entry_chunk_read(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info,
        char *buffer) {
    if (db->config->backend_type == STORAGE_DB_BACKEND_TYPE_MEMORY) {
        if (!memcpy(buffer, chunk_info->memory.chunk_data, chunk_info->chunk_length)) {
            return false;
        }
    } else {
        storage_channel_t *channel = chunk_info->file.shard_storage_channel;

        if (!storage_read(
                channel,
                buffer,
                chunk_info->chunk_length,
                chunk_info->file.chunk_offset)) {
            LOG_E(
                    TAG,
                    "[ENTRY_GET_CHUNK_INTERNAL] Failed to read chunk with offset <%u> long <%u> bytes (path <%s>)",
                    chunk_info->file.chunk_offset,
                    chunk_info->chunk_length,
                    channel->path);

            return false;
        }
    }

    return true;
}

bool storage_db_entry_chunk_write(
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
                    "[ENTRY_CHUNK_WRITE] Failed to write chunk with offset <%u> long <%u> bytes (path <%s>)",
                    chunk_info->file.chunk_offset,
                    chunk_info->chunk_length,
                    channel->path);

            return false;
        }
    }

    return true;
}

storage_db_chunk_info_t *storage_db_entry_key_chunk_get(
        storage_db_entry_index_t* entry_index,
        storage_db_chunk_index_t chunk_index) {

    return entry_index->key_chunks_info + chunk_index;
}

storage_db_chunk_info_t *storage_db_entry_value_chunk_get(
        storage_db_entry_index_t* entry_index,
        storage_db_chunk_index_t chunk_index) {

    return entry_index->value_chunks_info + chunk_index;
}

void storage_db_entry_index_status_increase_readers_counter(
        storage_db_entry_index_t* entry_index,
        storage_db_entry_index_status_t *old_status) {
    storage_db_entry_index_status_t old_status_internal;
    uint32_t old_cas_wrapper_ret = __sync_fetch_and_add(
            &entry_index->status._cas_wrapper,
            (uint32_t)1);

    // The MSB bit of _cas_wrapper is used for the deleted flag, if the readers_counter gets to 0x7FFFFFFF another lock
    // request would implicitly set the "deleted" flag to true.
    // Although this scenario would cause corruption it's not something we need to solve, it would mean that there are
    // +2 billion clients trying to access the same key which is not really possible with the current hardware
    // available or the current software implementation.
    // A way to solve the issue though is to have enough padding which would allow the counter to be decreased without
    // risking that other worker threads would increase it further causing the overflow.
    // In general just keeping the second MSB "free" for that scenario, the amount of padding required depends on the
    // amount of hardware threads that the cpu(s) are able to run in parallel.
    assert((old_cas_wrapper_ret & 0x7FFFFFFF) != 0x7FFFFFFF);

    // If the entry is marked as deleted reduce the readers counter to drop the lock
    old_status_internal._cas_wrapper = old_cas_wrapper_ret;
    if (unlikely(old_status_internal.deleted)) {
        old_cas_wrapper_ret = __sync_fetch_and_sub(
                &entry_index->status._cas_wrapper,
                (uint32_t)1);
    }

    if (likely(old_status)) {
        old_status->_cas_wrapper = old_cas_wrapper_ret;
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
    while((item = item_next) != NULL) {
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
        storage_db_entry_index_status_set_deleted(
                previous_entry_index,
                true,
                NULL);

        storage_db_entry_index_chunks_free(db, previous_entry_index);

        storage_db_entry_index_ring_buffer_free(db, previous_entry_index);
    } else {
        double_linked_list_item_t *item = double_linked_list_item_init();
        item->data = previous_entry_index;
        double_linked_list_push_item(
                storage_db_worker_deleting_entry_index_list(db),
                item);
    }
}

storage_db_entry_index_t *storage_db_get_entry_index(
        storage_db_t *db,
        char *key,
        size_t key_length) {
    hashtable_value_data_t memptr = 0;

    bool res = hashtable_mcmp_op_get(
            db->hashtable,
            key,
            key_length,
            &memptr);

    if (!res) {
        return NULL;
    }

    return (storage_db_entry_index_t *)memptr;
}

bool storage_db_set_entry_index(
        storage_db_t *db,
        char *key,
        size_t key_length,
        storage_db_entry_index_t *entry_index) {
    storage_db_entry_index_t *previous_entry_index = NULL;

    bool res = hashtable_mcmp_op_set(
            db->hashtable,
            key,
            key_length,
            (uintptr_t)entry_index,
            (uintptr_t*)&previous_entry_index);

    if (res && previous_entry_index != NULL) {
        storage_db_worker_mark_deleted_or_deleting_previous_entry_index(db, previous_entry_index);
    }

    return res;
}

bool storage_db_set_small_value(
        storage_db_t *db,
        char *key,
        size_t key_length,
        void *value,
        size_t value_length) {
    storage_db_entry_index_t *entry_index = NULL;
    bool return_res = false;

    assert(key_length <= STORAGE_DB_CHUNK_MAX_SIZE);
    assert(value_length <= STORAGE_DB_CHUNK_MAX_SIZE);

    entry_index = storage_db_entry_index_ring_buffer_new(db);
    if (!entry_index) {
        LOG_E(
                TAG,
                "Unable to fetch a database entry index");

        goto end;
    }

    // If the backend is in memory it's not necessary to write the key to the storage because it will never be used as
    // the only case in which the keys are read from the storage is when the database gets loaded from the disk at the
    // startup
    if (db->config->backend_type != STORAGE_DB_BACKEND_TYPE_MEMORY) {
        bool res = storage_db_entry_index_allocate_key_chunks(
                db,
                entry_index,
                key_length);

        if (!res) {
            LOG_E(
                    TAG,
                    "Unable to allocate database chunks for a key long <%lu> bytes",
                    key_length);

            goto end;
        }

        // Write the key
        bool res_write = storage_db_entry_chunk_write(
                db,
                storage_db_entry_key_chunk_get(entry_index, 0),
                0,
                key,
                key_length);

        if (!res_write) {
            LOG_E(
                    TAG,
                    "Unable to write the database chunks for a key long <%lu> bytes",
                    key_length);

            goto end;
        }
    }

    // The value stored is the key itself as it is
    bool res = storage_db_entry_index_allocate_value_chunks(
            db,
            entry_index,
            value_length);

    if (!res) {
        LOG_E(
                TAG,
                "Unable to allocate database chunks for a value long <%lu> bytes",
                value_length);
        goto end;
    }

    // Write the value
    bool res_write = storage_db_entry_chunk_write(
            db,
            storage_db_entry_value_chunk_get(entry_index, 0),
            0,
            value,
            value_length);

    if (!res_write) {
        LOG_E(
                TAG,
                "Unable to write the database chunks for a value long <%lu> bytes",
                value_length);

        goto end;
    }

    // Set the entry index
    bool result = storage_db_set_entry_index(
            db,
            key,
            value_length,
            entry_index);

    if (!result) {
        LOG_E(
                TAG,
                "Unable to update the database entry index");

        goto end;
    }

    return_res = true;

end:
    if (!return_res && entry_index) {
        storage_db_entry_index_free(db, entry_index);
    }

    return return_res;
}

bool storage_db_delete_entry_index(
        storage_db_t *db,
        char *key,
        size_t key_length) {
    storage_db_entry_index_t *current_entry_index = NULL;

    bool res = hashtable_mcmp_op_delete(
            db->hashtable,
            key,
            key_length,
            (uintptr_t*)&current_entry_index);

    if (res && current_entry_index != NULL) {
        storage_db_worker_mark_deleted_or_deleting_previous_entry_index(db, current_entry_index);
    }

    return res;
}
