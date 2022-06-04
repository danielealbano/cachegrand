#ifndef CACHEGRAND_WORKER_CONTEXT_H
#define CACHEGRAND_WORKER_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#define WORKER_LOOP_MAX_WAIT_TIME_MS 1000

// Circular dependency between the storage_db and the worker_context
typedef struct storage_db storage_db_t;

typedef struct worker_context worker_context_t;
struct worker_context {
    pthread_t pthread;
    bool_volatile_t *terminate_event_loop;
    bool_volatile_t aborted;
    bool_volatile_t running;
    uint32_t workers_count;
    uint32_t worker_index;
    uint32_t core_index;
    config_t *config;
    storage_db_t *db;
    struct {
        worker_stats_t internal;
        worker_stats_volatile_t shared;
    } stats;
    struct {
        void* context;
    } network;
    struct {
        void* context;
    } storage;
    struct {
        fiber_t *worker_storage_db_one_shot;
        fiber_t *timer_fiber;
        fiber_t **listeners_fibers;
    } fibers;
};

worker_context_t* worker_context_get();

void worker_context_set(
        worker_context_t *worker_context);

void worker_context_reset();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_CONTEXT_H
