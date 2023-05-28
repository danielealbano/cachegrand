#ifndef CACHEGRAND_WORKER_FIBER_STORAGE_DB_KEYS_EVICTION_H
#define CACHEGRAND_WORKER_FIBER_STORAGE_DB_KEYS_EVICTION_H

#ifdef __cplusplus
extern "C" {
#endif

#define WORKER_FIBER_STORAGE_DB_KEYS_EVICTION_WAIT_LOOP_MS 5l
#define WORKER_FIBER_STORAGE_DB_KEYS_EVICTION_CLOSE_TO_HARD_LIMIT_PERCENTAGE_THRESHOLD 0.99

void worker_fiber_storage_db_keys_eviction_fiber_entrypoint(
        void* user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_FIBER_STORAGE_DB_KEYS_EVICTION_H
