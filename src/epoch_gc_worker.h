#ifndef CACHEGRAND_EPOCH_GC_WORKER_H
#define CACHEGRAND_EPOCH_GC_WORKER_H

#ifdef __cplusplus
extern "C" {
#endif

#define EPOCH_GC_THREAD_LOOP_WAIT_TIME_MS 3
#define EPOCH_GC_THREAD_LOG_PRODUCER_PREFIX_TEMPLATE "[epoch gc %d]"
#define EPOCH_GC_THREAD_NAME_TEMPLATE "epoch_gc_%d"

typedef struct epoch_gc_worker_context epoch_gc_worker_context_t;
struct epoch_gc_worker_context {
    pthread_t pthread;
    epoch_gc_t *epoch_gc;
    volatile bool *terminate_event_loop;
    struct {
        uint64_t collected_objects;
    } stats;
};

bool epoch_gc_worker_should_terminate(
        epoch_gc_worker_context_t *context);

char *epoch_gc_worker_set_thread_name(
        epoch_gc_worker_context_t *context);

char* epoch_gc_worker_log_producer_set_early_prefix_thread(
        epoch_gc_worker_context_t *context);

void epoch_gc_worker_free_thread_list_cache(
        epoch_gc_thread_t **epoch_gc_thread_list_cache);

void epoch_gc_worker_build_thread_list_cache(
        epoch_gc_t *epoch_gc,
        epoch_gc_thread_t ***epoch_gc_thread_list_cache,
        uint64_t *epoch_gc_thread_list_change_epoch,
        uint32_t *epoch_gc_thread_list_length);

uint32_t epoch_gc_worker_collect_staged_objects(
        epoch_gc_thread_t **epoch_gc_thread_list_cache,
        uint32_t epoch_gc_thread_list_length,
        bool force);

void epoch_gc_worker_wait_for_epoch_gc_threads_termination(
        epoch_gc_thread_t **epoch_gc_thread_list_cache,
        uint32_t epoch_gc_thread_list_length);

void epoch_gc_worker_free_epoch_gc_thread(
        epoch_gc_thread_t **epoch_gc_thread_list_cache,
        uint32_t epoch_gc_thread_list_length);

void epoch_gc_worker_main_loop(
        epoch_gc_worker_context_t *epoch_gc_worker_context);

bool epoch_gc_worker_teardown(
        char *log_producer_early_prefix_thread,
        char *thread_name);

void* epoch_gc_worker_func(
        void* user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_EPOCH_GC_WORKER_H
