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

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_STORAGE_DB_SNAPSHOT_H
