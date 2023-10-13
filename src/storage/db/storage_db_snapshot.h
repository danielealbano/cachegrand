#ifndef CACHEGRAND_STORAGE_DB_SNAPSHOT_H
#define CACHEGRAND_STORAGE_DB_SNAPSHOT_H


#ifdef __cplusplus
extern "C" {
#endif

#define STORAGE_DB_SNAPSHOT_RDB_VERSION (9)
#define STORAGE_DB_SNAPSHOT_BLOCK_SIZE (64 * 1024)
#define STORAGE_DB_SNAPSHOT_REPORT_PROGRESS_EVERY_S (3)

struct storage_db_snapshot_entry_index_to_be_deleted {
    char *key;
    hashtable_bucket_index_t bucket_index;
    hashtable_key_length_t key_length;
    storage_db_entry_index_t *entry_index;
};
typedef struct storage_db_snapshot_entry_index_to_be_deleted storage_db_snapshot_entry_index_to_be_deleted_t;

void storage_db_snapshot_rdb_release_slice(
        storage_db_t *db,
        size_t slice_used_length);

void storage_db_snapshot_completed(
        storage_db_t *db,
        storage_db_snapshot_status_t status);

void storage_db_snapshot_failed(
        storage_db_t *db,
        bool during_preparation);

bool storage_db_snapshot_enough_keys_data_changed(
        storage_db_t *db);

bool storage_db_snapshot_should_run(
        storage_db_t *db);

void storage_db_snapshot_update_next_run_time(
        storage_db_t *db);

void storage_db_snapshot_skip_run(
        storage_db_t *db);

void storage_db_snapshot_wait_for_prepared(
        storage_db_t *db);

bool storage_db_snapshot_rdb_prepare(
        storage_db_t *db);

bool storage_db_snapshot_is_failed(
        storage_db_t *db);

bool storage_db_snapshot_rdb_ensure_prepared(
        storage_db_t *db);

bool storage_db_snapshot_rdb_write_value_header(
        storage_db_t *db,
        char *key,
        size_t key_length,
        storage_db_entry_index_t *entry_index);

bool storage_db_snapshot_rdb_write_value_string(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index);

bool storage_db_snapshot_rdb_write_database_number(
        storage_db_t *db,
        storage_db_database_number_t database_number);

bool storage_db_snapshot_rdb_process_entry_index(
        storage_db_t *db,
        char *key,
        size_t key_length,
        storage_db_entry_index_t *entry_index);

void storage_db_snapshot_mark_as_being_finalized(
        storage_db_t *db);

bool storage_db_snapshot_completed_successfully(
        storage_db_t *db);

bool storage_db_snapshot_rdb_completed_successfully(
        storage_db_t *db);

bool storage_db_snapshot_rdb_process_entry_index_to_be_deleted_queue(
        storage_db_t *db);

void storage_db_snapshot_rdb_flush_entry_index_to_be_deleted_queue(
        storage_db_t *db);

bool storage_db_snapshot_rdb_process_block(
        storage_db_t *db,
        storage_db_database_number_t *current_database_number,
        bool *last_block);

static inline bool storage_db_snapshot_lock_try_acquire(
        storage_db_t *db) {
    // Try to acquire the lock
    if (likely(spinlock_is_locked(&db->snapshot.spinlock))) {
        return false;
    }

    return spinlock_try_lock(&db->snapshot.spinlock);
}

static inline void storage_db_snapshot_release_lock(
        storage_db_t *db) {
    spinlock_unlock(&db->snapshot.spinlock);
}

static inline bool storage_db_snapshot_is_in_progress(
        storage_db_t *db) {
    return db->snapshot.status == STORAGE_DB_SNAPSHOT_STATUS_IN_PROGRESS;
}

void storage_db_snapshot_report_progress(
        storage_db_t *db);

static inline bool storage_db_snapshot_should_report_progress(
        storage_db_t *db) {
    return clock_monotonic_int64_ms() - db->snapshot.progress_reported_at_ms >
           STORAGE_DB_SNAPSHOT_REPORT_PROGRESS_EVERY_S * 1000;
}

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
            xalloc_alloc(sizeof(storage_db_snapshot_entry_index_to_be_deleted_t));
    if (!data) {
        return false;
    }

    data->bucket_index = bucket_index;
    data->key = key_dup;
    data->key_length = key_length;
    data->entry_index = entry_index;

    result = queue_mpmc_push(db->snapshot.entry_index_to_be_deleted_queue, data);

    if (!result) {
        xalloc_free(data);
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
