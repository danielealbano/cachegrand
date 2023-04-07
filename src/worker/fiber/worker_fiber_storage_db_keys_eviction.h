#ifndef CACHEGRAND_WORKER_FIBER_STORAGE_DB_KEYS_EVICTION_H
#define CACHEGRAND_WORKER_FIBER_STORAGE_DB_KEYS_EVICTION_H

#ifdef __cplusplus
extern "C" {
#endif

#define WORKER_FIBER_STORAGE_DB_KEYS_EVICTION_WAIT_LOOP_MS 1l

void worker_fiber_storage_db_keys_eviction_fiber_entrypoint(
        void* user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_FIBER_STORAGE_DB_KEYS_EVICTION_H
