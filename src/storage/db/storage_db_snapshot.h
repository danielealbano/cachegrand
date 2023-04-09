#ifndef CACHEGRAND_STORAGE_DB_SNAPSHOT_H
#define CACHEGRAND_STORAGE_DB_SNAPSHOT_H

#ifdef __cplusplus
extern "C" {
#endif
bool storage_db_snapshot_lock_try_acquire(
        storage_db_t *db);

void storage_db_snapshot_release_lock(
            storage_db_t *db);

bool storage_db_snapshot_rdb_process_block(
        storage_db_t *db,
        bool *last_block);

static inline bool storage_db_snapshot_queue_entry_index_to_be_deleted(
        storage_db_t *db,
        hashtable_bucket_index_t bucket_index,
        char *key,
        size_t key_length,
        storage_db_entry_index_t *entry_index) {
    bool result;
    char *key_dup = xalloc_alloc_zero(key_length + 1);
    memcpy(key_dup, key, key_length);

    storage_db_snapshot_entry_index_to_be_deleted_t *data =
            ffma_mem_alloc(sizeof(storage_db_snapshot_entry_index_to_be_deleted_t));
    data->bucket_index = bucket_index;
    data->key = key_dup;
    data->key_length = key_length;
    data->entry_index = entry_index;

    result = queue_mpmc_push(db->snapshot.entry_index_to_be_deleted_queue, data);

    if (!result) {
        ffma_mem_free(data);
        xalloc_free(key_dup);
    }

    return result;
}

static inline bool storage_db_snapshot_should_entry_index_be_processed_block_not_processed(
        storage_db_t *db,
        hashtable_bucket_index_t bucket_index) {
    MEMORY_FENCE_LOAD();
    return bucket_index / STORAGE_DB_SNAPSHOT_BLOCK_SIZE >= db->snapshot.block_index;
}

static inline bool storage_db_snapshot_should_entry_index_be_processed_creation_time(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index) {
    return entry_index->created_time_ms < db->snapshot.start_time_ms;
}

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_STORAGE_DB_SNAPSHOT_H
