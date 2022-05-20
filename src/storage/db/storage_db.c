#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <fcntl.h>
#include <pow2.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"
#include "data_structures/hashtable/mcmp/hashtable_op_get.h"
#include "data_structures/hashtable/mcmp/hashtable_op_set.h"
#include "data_structures/hashtable/mcmp/hashtable_op_delete.h"
#include "slab_allocator.h"
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
    // Initialize the hashtable configuration
    hashtable_config_t* hashtable_config = hashtable_mcmp_config_init();
    if (!hashtable_config) {
        // TODO: log
        return NULL;
    }
    hashtable_config->can_auto_resize = false;
    hashtable_config->initial_size = pow2_next(config->max_keys);

    // Initialize the hashtable
    hashtable_t *hashtable = hashtable_mcmp_init(hashtable_config);
    if (!hashtable) {
        // TODO: log
        hashtable_mcmp_config_free(hashtable_config);
        return NULL;
    }

    // Initialize the db wrapper structure
    storage_db_t *db = slab_allocator_mem_alloc_zero(sizeof(storage_db_t));
    if (!db) {
        hashtable_mcmp_config_free(hashtable_config);
        // TODO: log
        return NULL;
    }

    // Sets up all the db related information
    db->config = config;
    db->shards.active_per_worker = slab_allocator_mem_alloc_zero(sizeof(storage_db_shard_t*) * workers_count);
    db->shards.new_index = 0;
    db->shards.opened_shards = double_linked_list_init();
    spinlock_init(&db->shards.write_spinlock);
    db->hashtable = hashtable;

    return db;
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

bool storage_db_shard_ensure_size_preallocated(
        storage_channel_t *storage_channel,
        uint32_t size_mb) {
    return storage_fallocate(storage_channel, 0, 0, size_mb * 1024 * 1024);
}

storage_db_shard_t *storage_db_shard_get_active_per_current_worker(
        storage_db_t *db) {
    worker_context_t *worker_context = worker_context_get();
    uint32_t worker_index = worker_context->worker_index;

    return db->shards.active_per_worker[worker_index];
}

bool storage_db_shard_new_is_needed(
        storage_db_shard_t *shard,
        size_t chunk_length) {
    return shard->offset + chunk_length > shard->size;
}

bool storage_db_shard_allocate_chunk(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info,
        size_t chunk_length) {
    storage_db_shard_t *shard;

    if ((shard = storage_db_shard_get_active_per_current_worker(db)) != NULL) {
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

        if ((shard = storage_db_new_active_shard_per_current_worker(db)) == false) {
            LOG_E(
                    TAG,
                    "Unable to allocate a new shard for worker <%u>",
                    worker_context_get()->worker_index);
            return false;
        }
    }

    chunk_info->shard_storage_channel = shard->storage_channel;
    chunk_info->chunk_offset = shard->offset;
    chunk_info->chunk_length = chunk_length;

    shard->offset += chunk_length;

    return true;
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

    if (!storage_db_shard_ensure_size_preallocated(storage_channel, shard_size_mb)) {
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

    spinlock_lock(&db->shards.write_spinlock, true);

    storage_db_shard_t *shard = storage_db_shard_new(
            db->shards.new_index,
            storage_db_shard_build_path(db->config->backend.file.basedir_path, db->shards.new_index),
            db->config->backend.file.shard_size_mb);
    db->shards.new_index++;
    db->shards.active_per_worker[worker_index] = shard;

    double_linked_list_item_t *item = double_linked_list_item_init();
    item->data = shard;
    double_linked_list_push_item(db->shards.opened_shards, item);

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
    double_linked_list_item_t* item = NULL;
    while((item = db->shards.opened_shards->head) != NULL) {
        storage_db_shard_free(db, (storage_db_shard_t*)item->data);
        double_linked_list_remove_item(db->shards.opened_shards, item);
        double_linked_list_item_free(item);
    }
}

void storage_db_free(
        storage_db_t *db) {
    double_linked_list_free(db->shards.opened_shards);
    hashtable_mcmp_free(db->hashtable);
    storage_db_config_free(db->config);
    slab_allocator_mem_free(db);
}

storage_db_entry_index_t *storage_db_entry_index_new() {
    return slab_allocator_mem_alloc_zero(sizeof(storage_db_entry_index_t));
}

storage_db_entry_index_t *storage_db_entry_index_allocate_key_chunks(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index,
        size_t key_length) {
    uint32_t chunk_count = ceil((double)key_length / (double)STORAGE_DB_CHUNK_MAX_SIZE);

    entry_index->key_length = key_length;
    entry_index->key_chunks_info = slab_allocator_mem_alloc(sizeof(storage_db_chunk_info_t) * chunk_count);
    entry_index->key_chunks_count = chunk_count;

    size_t remaining_length = key_length;
    for(storage_db_chunk_index_t chunk_index = 0; chunk_index < entry_index->key_chunks_count; chunk_index++) {
        storage_db_chunk_info_t *chunk_info = storage_db_entry_key_chunk_get(entry_index, chunk_index);
        storage_db_shard_allocate_chunk(db, chunk_info, min(remaining_length, STORAGE_DB_CHUNK_MAX_SIZE));
        remaining_length -= STORAGE_DB_CHUNK_MAX_SIZE;
    }
}

storage_db_entry_index_t *storage_db_entry_index_allocate_value_chunks(
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
        storage_db_shard_allocate_chunk(db, chunk_info, min(remaining_length, STORAGE_DB_CHUNK_MAX_SIZE));
        remaining_length -= STORAGE_DB_CHUNK_MAX_SIZE;
    }
}

storage_db_entry_index_t *storage_db_entry_index_free(
        storage_db_entry_index_t *entry_index) {

    if (entry_index->key_chunks_info) {
        slab_allocator_mem_free(entry_index->key_chunks_info);
    }

    if (entry_index->value_chunks_info) {
        slab_allocator_mem_free(entry_index->value_chunks_info);
    }

    slab_allocator_mem_free(entry_index);
}

bool storage_db_entry_chunk_read(
        storage_db_chunk_info_t *chunk_info,
        char *buffer) {

    storage_channel_t *channel = chunk_info->shard_storage_channel;

    if (!storage_read(
            channel,
            buffer,
            chunk_info->chunk_length,
            chunk_info->chunk_offset)) {
        LOG_E(
                TAG,
                "[ENTRY_GET_CHUNK_INTERNAL] Failed to read chunk with offset <%u> long <%u> bytes (path <%s>)",
                chunk_info->chunk_offset,
                chunk_info->chunk_length,
                channel->path);

        return false;
    }

    return true;
}

bool storage_db_entry_chunk_write(
        storage_db_chunk_info_t *chunk_info,
        char *buffer) {

    storage_channel_t *channel = chunk_info->shard_storage_channel;

    if (!storage_write(
            channel,
            buffer,
            chunk_info->chunk_length,
            chunk_info->chunk_offset)) {
        LOG_E(
                TAG,
                "[ENTRY_CHUNK_WRITE] Failed to write chunk with offset <%u> long <%u> bytes (path <%s>)",
                chunk_info->chunk_offset,
                chunk_info->chunk_length,
                channel->path);

        return false;
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

storage_db_entry_index_t *storage_db_get(
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

bool storage_db_set(
        storage_db_t *db,
        char *key,
        size_t key_length,
        storage_db_entry_index_t *entry_index) {
    bool res = hashtable_mcmp_op_set(
            db->hashtable,
            key,
            key_length,
            (uintptr_t)entry_index);

    return res;
}

bool storage_db_delete(
        storage_db_t *db,
        char *key,
        size_t key_length) {
    return hashtable_mcmp_op_delete(
            db->hashtable,
            key,
            key_length);
}
