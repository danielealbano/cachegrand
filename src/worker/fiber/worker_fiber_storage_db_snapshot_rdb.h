#ifndef CACHEGRAND_WORKER_FIBER_STORAGE_DB_SNAPSHOT_RDB_H
#define CACHEGRAND_WORKER_FIBER_STORAGE_DB_SNAPSHOT_RDB_H

#ifdef __cplusplus
extern "C" {
#endif

#define WORKER_FIBER_STORAGE_DB_SNAPSHOT_RDB_WAIT_LOOP_MS 10l

void worker_fiber_storage_db_keys_eviction_fiber_entrypoint(
        void* user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_FIBER_STORAGE_DB_SNAPSHOT_RDB_H
