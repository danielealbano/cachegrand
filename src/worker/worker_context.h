#ifndef CACHEGRAND_WORKER_CONTEXT_H
#define CACHEGRAND_WORKER_CONTEXT_H

#ifdef __cplusplus
#include <atomic>
#else
#include <stdatomic.h>
#endif
#include "memory_fences.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WORKER_LOOP_MAX_WAIT_TIME_MS 1000

// Circular dependency between the storage_db and the worker_context
typedef struct storage_db storage_db_t;

typedef struct worker_context worker_context_t;
struct worker_context {
    pthread_t pthread;
    void *interface_context;
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
    double_linked_list_t *fibers;
    bool_volatile_t *storage_db_loaded;
};

worker_context_t* worker_context_get();

void worker_context_set(
        worker_context_t *worker_context);

void worker_context_reset();

static inline bool worker_is_running(
        worker_context_t *worker_context) {
    MEMORY_FENCE_LOAD();
    return worker_context->running;
}

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_CONTEXT_H
