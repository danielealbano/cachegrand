#ifndef CACHEGRAND_SIGNALS_SUPPORT_H
#define CACHEGRAND_SIGNALS_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#define SIGNALS_SUPPORT_NAME_BUFFER_SIZE 20
#define SIGNALS_SUPPORT_NAME_WRAPPER(SIGNAL_NUMBER) \
    ({ \
        char *signal_name; \
        char signal_name_buffer[SIGNALS_SUPPORT_NAME_BUFFER_SIZE] = {0}; \
        signal_name = signals_support_name(SIGNAL_NUMBER, signal_name_buffer, sizeof(signal_name_buffer)); \
        if (!signal_name) { \
            signal_name = unknown_signal_name; \
        } \
        \
        signal_name; \
    })

typedef void (*sighandler_t) (int);

extern char *unknown_signal_name;

void signals_support_handler_sigsegv_fatal(int signal_number);
void signals_support_register_sigsegv_fatal_handler();
char* signals_support_name(
        int signal_number,
        char* buffer,
        size_t buffer_size);
bool signals_support_register_signal_handler(
        int signal_number,
        sighandler_t signal_handler,
        struct sigaction* previous_action);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_SIGNALS_SUPPORT_H
