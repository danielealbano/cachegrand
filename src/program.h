#ifndef CACHEGRAND_PROGRAM_H
#define CACHEGRAND_PROGRAM_H

#ifdef __cplusplus
extern "C" {
#endif

#define PROGRAM_NAME            "cachegrand"
#define PROGRAM_VERSION         CACHEGRAND_CMAKE_CONFIG_VERSION_GIT

typedef struct program_context program_context_t;
struct program_context {
    bool use_slab_allocator;
    bool slab_allocator_inited;
    config_t* config;
    uint16_t* selected_cpus;
    uint16_t selected_cpus_count;
    hashtable_t* hashtable;
    uint32_t workers_count;
    worker_user_data_t* workers_user_data;
};

void program_signal_handlers(
        int sig);

void program_register_signal_handlers();

worker_user_data_t* program_workers_initialize(
        volatile bool *terminate_event_loop,
        config_t* config,
        uint32_t workers_count);

bool* program_get_terminate_event_loop();

void program_request_terminate(
        volatile bool *terminate_event_loop);

bool program_should_terminate(
        volatile bool *terminate_event_loop);

void program_wait_loop(
        volatile bool *terminate_event_loop);

void program_workers_cleanup(
        worker_user_data_t* workers_user_data,
        uint32_t workers_count);

int program_main(
        int argc,
        char** argv);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_PROGRAM_H
