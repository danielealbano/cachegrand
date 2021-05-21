#ifndef CACHEGRAND_SIGNAL_HANDLER_THREAD_H
#define CACHEGRAND_SIGNAL_HANDLER_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#define SIGNAL_HANDLER_THREAD_LOOP_MAX_WAIT_TIME_MS 500
#define SIGNAL_HANDLER_THREAD_LOG_PRODUCER_PREFIX_FORMAT_STRING "[signal handler thread]"


typedef struct signal_handler_thread_context signal_handler_thread_context_t;
struct signal_handler_thread_context {
    pthread_t pthread;
    volatile bool *terminate_event_loop;
};

void* signal_handler_thread_func(
        void* user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_SIGNAL_HANDLER_THREAD_H
