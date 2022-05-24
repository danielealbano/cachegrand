#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <fcntl.h>
#include <pow2.h>
#include <stdatomic.h>
#include <memory_fences.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
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

    // Initialize the per worker set of information
    workers = slab_allocator_mem_alloc_zero(sizeof(storage_db_worker_t) * workers_count);
    if (!workers) {
        LOG_E(TAG, "Unable to allocate memory for the per worker configurations");
        goto fail;
    }

    // Initialize the per worker needed information
    for(uint32_t worker_index = 0; worker_index < workers_count; worker_index++) {
        small_circular_queue_t *entry_index_ringbuffer =
                small_circular_queue_init(STORAGE_DB_WORKER_ENTRY_INDEX_RING_BUFFER_SIZE);

        if (entry_index_ringbuffer) {
            LOG_E(TAG, "Unable to allocate memory for the entry index ring buffer per worker");
            goto fail;
        }

        workers[worker_index].entry_index_ringbuffer = entry_index_ringbuffer;
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
    db->shards.new_index = 0;
    db->shards.opened_shards = double_linked_list_init();
    spinlock_init(&db->shards.write_spinlock);

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
            small_circular_queue_free(workers[worker_index].entry_index_ringbuffer);
        }

        slab_allocator_mem_free(workers);
    }

    // This condition is actually always false but it's here for clarity
    if (db) {
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

bool storage_db_shard_ensure_size_preallocated(
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

small_circular_queue_t *storage_db_worker_entry_index_ringbuffer(
        storage_db_t *db) {
    worker_context_t *worker_context = worker_context_get();
    uint32_t worker_index = worker_context->worker_index;

    return db->workers[worker_index].entry_index_ringbuffer;
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

storage_db_entry_index_t *storage_db_entry_index_ringbuffer_new(
        storage_db_t *db) {
    // The storage_db_entry_indexes are stored into the hashtable and used as reference to keep track of the
    // key and chunks of the value mapped to the index.
    // When the entry on the hashtable is updated, the existing one can't freed right away even if the readers_counter
    // is set to zero because potentially one of the worker threads running on a different core might just have
    // fetched the pointer and it might be trying to access it.
    // To avoid a potential issue all the storage_db_entry_index are fetched from a ring buffer which is large.
    // The ring buffer is used in a bit of an odd way, the entries are feteched from it only if the underlying
    // circular queue is full, because this will guarantee that enough time will have passed for any context
    // switch happened in between reading the value from the hashtable and checking if the value is still viable.
    // The size of the ring buffer (so when it's full) is dictated by having enough room to allow any CPU that
    // might be trying to access the value stored in the hashtable to be checked.
    // if the queue is not full then a new entry_index is allocated

    small_circular_queue_t *scb = storage_db_worker_entry_index_ringbuffer(db);

    if (small_circular_queue_is_full(scb)) {
        return small_circular_queue_dequeue(scb);
    } else {
        return storage_db_entry_index_new();
    }
}

void storage_db_entry_index_ringbuffer_free(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index) {
    small_circular_queue_t *scb = storage_db_worker_entry_index_ringbuffer(db);

    // If the queue is full, the entry in the head can be dequeued and freed because it means it has lived enough
    if (small_circular_queue_is_full(scb)) {
        storage_db_entry_index_t *entry_index_to_free = small_circular_queue_dequeue(scb);
        storage_db_entry_index_free(entry_index_to_free);
    }

    small_circular_queue_enqueue(scb, entry_index);
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

        if (!storage_db_shard_allocate_chunk(db, chunk_info, min(remaining_length, STORAGE_DB_CHUNK_MAX_SIZE))) {
            slab_allocator_mem_free(entry_index->key_chunks_info);
            return NULL;
        }

        remaining_length -= STORAGE_DB_CHUNK_MAX_SIZE;
    }

    return entry_index;
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

        if (!storage_db_shard_allocate_chunk(db, chunk_info, min(remaining_length, STORAGE_DB_CHUNK_MAX_SIZE))) {
            slab_allocator_mem_free(entry_index->value_chunks_info);
            return NULL;
        }

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

void storage_db_entry_index_status_update_prep_expected_and_new(
        storage_db_entry_index_t* entry_index,
        storage_db_entry_index_status_t *expected_status,
        storage_db_entry_index_status_t *new_status) {
    expected_status->_cas_wrapper = entry_index->status._cas_wrapper;
    new_status->_cas_wrapper = expected_status->_cas_wrapper;
}

bool storage_db_entry_index_status_try_compare_and_swap(
        storage_db_entry_index_t* entry_index,
        storage_db_entry_index_status_t *expected_status,
        storage_db_entry_index_status_t *new_status) {
    return atomic_compare_exchange_strong(
            &entry_index->status._cas_wrapper,
            &expected_status->_cas_wrapper,
            new_status->_cas_wrapper);
}

void storage_db_entry_index_status_load(
        storage_db_entry_index_t* entry_index,
        storage_db_entry_index_status_t *status) {
    status->_cas_wrapper = atomic_load(&entry_index->status._cas_wrapper);
}

bool storage_db_entry_index_status_try_acquire_reader_lock(
        storage_db_entry_index_t* entry_index,
        storage_db_entry_index_status_t *new_status) {
    storage_db_entry_index_status_t expected_status;
    storage_db_entry_index_status_update_prep_expected_and_new(
            entry_index,
            &expected_status,
            new_status);
    new_status->deleted = false;
    new_status->readers_counter++;

    return storage_db_entry_index_status_try_compare_and_swap(
            entry_index,
            &expected_status,
            new_status);
}

bool storage_db_entry_index_status_try_free_reader_lock(
        storage_db_entry_index_t* entry_index,
        storage_db_entry_index_status_t *new_status) {
    storage_db_entry_index_status_t expected_status;
    storage_db_entry_index_status_update_prep_expected_and_new(
            entry_index,
            &expected_status,
            new_status);
    new_status->readers_counter--;

    return storage_db_entry_index_status_try_compare_and_swap(
            entry_index,
            &expected_status,
            new_status);
}

bool storage_db_entry_index_status_try_set_deleted(
        storage_db_entry_index_t* entry_index,
        storage_db_entry_index_status_t *new_status) {
    storage_db_entry_index_status_t expected_status;
    storage_db_entry_index_status_update_prep_expected_and_new(
            entry_index,
            &expected_status,
            new_status);
    expected_status.deleted = false;
    new_status->deleted = true;

    return storage_db_entry_index_status_try_compare_and_swap(
            entry_index,
            &expected_status,
            new_status);
}

bool storage_db_entry_index_status_get_deleted(
        storage_db_entry_index_t* entry_index) {
    storage_db_entry_index_status_t status;
    storage_db_entry_index_status_load(entry_index, &status);

    return status.deleted;
}

uint16_t storage_db_entry_index_status_get_readers_counter(
        storage_db_entry_index_t* entry_index) {
    storage_db_entry_index_status_t status;
    storage_db_entry_index_status_load(entry_index, &status);

    return status.readers_counter;
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
    storage_db_entry_index_status_t entry_index_status;
    storage_db_entry_index_t *previous_entry_index = NULL;

    // TODO: fetch previous value and mark it as deleted, if the readers count is set to 0
    bool res = hashtable_mcmp_op_set(
            db->hashtable,
            key,
            key_length,
            (uintptr_t)entry_index,
            (uintptr_t*)&previous_entry_index);

    if (res && previous_entry_index != NULL) {

        if (storage_db_entry_index_status_try_set_deleted(
                previous_entry_index,
                &entry_index_status)) {

        }
    }

    return res;
}

bool storage_db_delete(
        storage_db_t *db,
        char *key,
        size_t key_length) {
    storage_db_entry_index_t *previous_entry_index = NULL;

    // TODO: fetch previous value
    bool res = hashtable_mcmp_op_delete(
            db->hashtable,
            key,
            key_length,
            (uintptr_t*)&previous_entry_index);

    if (res && previous_entry_index != NULL) {

    }

    return res;
}
