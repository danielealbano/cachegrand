#ifndef CACHEGRAND_PROGRAM_H
#define CACHEGRAND_PROGRAM_H

#ifdef __cplusplus
extern "C" {
#endif

#define CACHEGRAND_MIN_KERNEL_VERSION "5.7.0"

typedef struct program_context program_context_t;
struct program_context {
    config_t *config;
    uint16_t *selected_cpus;
    uint16_t selected_cpus_count;
    storage_db_t *db;
    uint32_t workers_count;
    worker_context_t *workers_context;
    signal_handler_thread_context_t *signal_handler_thread_context;
    uint32_t epoch_gc_workers_count;
    epoch_gc_worker_context_t *epoch_gc_workers_context;
    bool_volatile_t storage_db_loaded;
    bool_volatile_t workers_terminate_event_loop;
    bool_volatile_t program_terminate_event_loop;
};

program_context_t *program_get_context();

void program_reset_context();

bool program_config_setup_storage_db(
        program_context_t* program_context);

bool program_epoch_gc_workers_initialize(
        program_context_t *program_context);

void program_epoch_gc_workers_cleanup(
        epoch_gc_worker_context_t *epoch_gc_workers_context,
        uint32_t epoch_gc_workers_count);

void program_workers_initialize_count(
        program_context_t *program_context);

worker_context_t* program_workers_initialize_context(
        program_context_t *program_context);

void program_request_terminate(
        volatile bool *terminate_event_loop);

bool program_should_terminate(
        const volatile bool *terminate_event_loop);

void program_wait_loop(
        worker_context_t *worker_context,
        uint32_t workers_count,
        const bool_volatile_t *terminate_event_loop);

void program_workers_cleanup(
        worker_context_t *worker_context,
        uint32_t workers_count);

bool program_setup_pidfile(
        program_context_t *program_context);

bool program_initialize_module(
        program_context_t* program_context);

bool program_cleanup_module(
        program_context_t* program_context);

bool program_config_thread_affinity_set_selected_cpus(
        program_context_t *program_context);

void program_cleanup(
        program_context_t *program_context);

int program_main(
        int argc,
        char** argv);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_PROGRAM_H
