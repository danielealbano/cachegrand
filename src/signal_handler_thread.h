#ifndef CACHEGRAND_SIGNAL_HANDLER_THREAD_H
#define CACHEGRAND_SIGNAL_HANDLER_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#define SIGNAL_HANDLER_THREAD_LOOP_MAX_WAIT_TIME_MS 3
#define SIGNAL_HANDLER_THREAD_LOG_PRODUCER_PREFIX_FORMAT_STRING "[signal handler thread]"
#define SIGNAL_HANDLER_THREAD_NAME "signal_handler"

typedef struct signal_handler_thread_context signal_handler_thread_context_t;

extern signal_handler_thread_context_t *signal_handler_thread_internal_context;
extern int signal_handler_thread_managed_signals[];
extern uint8_t signal_handler_thread_managed_signals_count;

struct signal_handler_thread_context {
    pthread_t pthread;
    volatile bool *workers_terminate_event_loop;
    volatile bool *program_terminate_event_loop;
};

void signal_handler_thread_handle_signal(
        int signal_number);

bool signal_handler_thread_should_terminate(
        signal_handler_thread_context_t *context);

void signal_handler_thread_register_signal_handlers(
        sigset_t *waitset);

char* signal_handler_thread_log_producer_set_early_prefix_thread();

void signal_handler_thread_main_loop(
        sigset_t *waitset,
        struct timespec *timeout);

void signal_handler_thread_setup_timeout(
        struct timespec *timeout);

void signal_handler_thread_set_thread_name();

void signal_handler_thread_mask_signals(
        sigset_t *waitset);

bool signal_handler_thread_teardown(
        sigset_t *waitset,
        char *log_producer_early_prefix_thread);

void* signal_handler_thread_func(
        void* user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_SIGNAL_HANDLER_THREAD_H
