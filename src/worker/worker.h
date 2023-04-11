#ifndef CACHEGRAND_WORKER_H
#define CACHEGRAND_WORKER_H

#ifdef __cplusplus
extern "C" {
#endif

#define WORKER_LOG_PRODUCER_PREFIX_FORMAT_STRING "[worker][id: %02u][cpu: %02d]"

char* worker_log_producer_set_early_prefix_thread(
        worker_context_t *worker_context);

void worker_setup_context(
        worker_context_t *worker_context,
        timespec_t *started_on_timestamp,
        uint32_t workers_count,
        uint32_t worker_index,
        bool_volatile_t *terminate_event_loop,
        bool_volatile_t *storage_db_loaded,
        config_t *config,
        storage_db_t *db);

bool worker_should_terminate(
        worker_context_t *worker_context);

void worker_request_terminate(
        worker_context_t *worker_context);

uint32_t worker_thread_set_affinity(
        uint32_t worker_index);

void worker_wait_running(
        worker_context_t *worker_context);

void worker_set_running(
        worker_context_t *worker_context,
        bool running);

void worker_set_aborted(
        worker_context_t *worker_context,
        bool aborted);

void* worker_thread_func(
        void* user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_H
