#ifndef CACHEGRAND_PROGRAM_H
#define CACHEGRAND_PROGRAM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct program_context program_context_t;
struct program_context {
    bool use_slab_allocator;
    bool slab_allocator_inited;
    config_t *config;
    uint16_t *selected_cpus;
    uint16_t selected_cpus_count;
    hashtable_t *hashtable;
    uint32_t workers_count;
    worker_context_t *workers_context;
    signal_handler_thread_context_t *signal_handler_thread_context;
};

worker_context_t* program_workers_initialize(
        volatile bool *terminate_event_loop,
        program_context_t *program_context);

bool* program_get_terminate_event_loop();

void program_request_terminate(
        volatile bool *terminate_event_loop);

bool program_should_terminate(
        const volatile bool *terminate_event_loop);

void program_wait_loop(
        const volatile bool *terminate_event_loop);

void program_workers_cleanup(
        worker_context_t* workers_user_data,
        uint32_t workers_count);

bool program_config_thread_affinity_set_selected_cpus(
        program_context_t* program_context);

int program_main(
        int argc,
        char** argv);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_PROGRAM_H
