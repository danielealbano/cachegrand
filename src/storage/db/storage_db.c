#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>

#include "misc.h"
#include "exttypes.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "spinlock.h"
#include "log/log.h"
#include "slab_allocator.h"
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
        storage_db_config_t *config) {
    storage_db_t *db = slab_allocator_mem_alloc_zero(sizeof(storage_db_t));

    // TODO: can this actually happen? If memory can't be allocated the application with the current implementation
    //       should just crash
    if (!db) {
        return NULL;
    }

    db->config = config;

    // TODO: this should be done via shards, not embedded here
    db->shard_channel = NULL;

    return db;
}

storage_channel_t *storage_db_shard_open_or_create(
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

bool storage_db_open(
        storage_db_t *db) {
    // TODO: the shard index is currently hard coded but this should be determined automatically
    // TODO: the db open should not open or create a shard, this should be done when writing and reading
    storage_channel_t *shard_channel = storage_db_shard_open_or_create(
            storage_db_shard_build_path(db->config->backend.file.basedir_path, 0),
            true);

    if (shard_channel) {
        if (!storage_db_shard_ensure_size_preallocated(shard_channel, db->config->shard_size_mb)) {
            storage_close(shard_channel);
            shard_channel = NULL;
        }
    }

    if (!shard_channel) {
        return false;
    }

    db->shard_channel = shard_channel;
    db->shard_index = 0;
    db->shard_offset = 0;

    return true;
}
bool storage_db_close(
        storage_db_t *db) {
    // TODO: should iterate on all the active shards and close them
    // TODO: should also ensure that all the in-flight data being written are actually written
    // TODO: should close the shards properly writing the footer to be able to carry out a quick read
    storage_close(db->shard_channel);
}

void storage_db_free(
        storage_db_t *db) {
    storage_db_config_free(db->config);
    slab_allocator_mem_free(db);
}

void storage_db_shard_allocate_chunk(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info,
        size_t chunk_length) {

    chunk_info->shard_index = db->shard_index;
    chunk_info->chunk_offset = db->shard_offset;
    chunk_info->chunk_length = chunk_length;

    db->shard_offset += chunk_length;
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
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info,
        char *buffer) {

    // TODO: should get the correct shard from the db structure depending on the worker index
    storage_channel_t *channel = db->shard_channel;

    if (!storage_read(
            channel,
            buffer,
            chunk_info->chunk_length,
            chunk_info->chunk_offset)) {
        LOG_E(
                TAG,
                "[ENTRY_GET_CHUNK_INTERNAL] Failed to read chunk with offset <%u> long <%u> bytes in shard <%u> (path <%s>)",
                chunk_info->chunk_offset,
                chunk_info->chunk_length,
                chunk_info->shard_index,
                channel->path);

        return false;
    }

    return true;
}

bool storage_db_entry_chunk_write(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info,
        char *buffer) {

    // TODO: should get the correct shard from the db structure depending on the worker index
    storage_channel_t *channel = db->shard_channel;

    if (!storage_write(
            channel,
            buffer,
            chunk_info->chunk_length,
            chunk_info->chunk_offset)) {
        LOG_E(
                TAG,
                "[ENTRY_CHUNK_WRITE] Failed to write chunk with offset <%u> long <%u> bytes in shard <%u> (path <%s>)",
                chunk_info->chunk_offset,
                chunk_info->chunk_length,
                chunk_info->shard_index,
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
