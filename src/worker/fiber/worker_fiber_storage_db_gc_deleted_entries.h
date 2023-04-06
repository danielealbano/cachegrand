#ifndef CACHEGRAND_WORKER_FIBER_STORAGE_DB_GC_DELETED_ENTRIES_H
#define CACHEGRAND_WORKER_FIBER_STORAGE_DB_GC_DELETED_ENTRIES_H

#ifdef __cplusplus
extern "C" {
#endif

#define WORKER_FIBER_GC_DELETED_STORAGE_DB_ENTRIES_TIMER_LOOP_MS 50l

void worker_fiber_storage_db_gc_deleted_entries_fiber_entrypoint(
        void *user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_FIBER_STORAGE_DB_GC_DELETED_ENTRIES_H
