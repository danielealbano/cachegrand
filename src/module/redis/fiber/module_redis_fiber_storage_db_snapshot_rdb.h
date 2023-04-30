#ifndef CACHEGRAND_MODULE_REDIS_FIBER_STORAGE_DB_SNAPSHOT_RDB_H
#define CACHEGRAND_MODULE_REDIS_FIBER_STORAGE_DB_SNAPSHOT_RDB_H

#ifdef __cplusplus
extern "C" {
#endif

#define MODULE_REDIS_FIBER_STORAGE_DB_SNAPSHOT_RDB_WAIT_LOOP_MS 10l

void module_redis_fiber_storage_db_snapshot_rdb_fiber_entrypoint(
        void* user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_REDIS_FIBER_STORAGE_DB_SNAPSHOT_RDB_H
