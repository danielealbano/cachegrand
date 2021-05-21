#ifndef CACHEGRAND_LOG_H
#define CACHEGRAND_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <log/log_debug.h>
#include <stdio.h>

#define LOG_SINK_REGISTERED_MAX             4
#define LOG_MESSAGE_TIMESTAMP_MAX_LENGTH    20

#define LOG_E(tag, ...) \
    log_message(tag, LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_W(tag, ...) \
    log_message(tag, LOG_LEVEL_WARNING, __VA_ARGS__)
#define LOG_I(tag, ...) \
    log_message(tag, LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_V(tag, ...) \
    log_message(tag, LOG_LEVEL_VERBOSE, __VA_ARGS__)
#define LOG_E_OS_ERROR(tag) \
    log_message_print_os_error(tag)

#ifndef DEBUG
#define LOG_D(...) /* Internal debug logs disabled */
#define LOG_DI(...) /* Debug logs disabled, verbose is appropriate for release builds */
#else
#define LOG_D(tag, ...) \
    log_message(tag, LOG_LEVEL_DEBUG, __VA_ARGS__)
#endif // DEBUG == 1

enum log_level {
    LOG_LEVEL_DEBUG_INTERNALS = 0x01,
    LOG_LEVEL_DEBUG = 0x02,
    LOG_LEVEL_VERBOSE = 0x04,
    LOG_LEVEL_INFO = 0x08,
    LOG_LEVEL_WARNING = 0x10,
    LOG_LEVEL_ERROR = 0x20,
    LOG_LEVEL_MAX
};
typedef enum log_level log_level_t;

#define LOG_LEVEL_ALL (((LOG_LEVEL_MAX - 1u) * 2u) - 1u)

void log_set_early_prefix_thread(
        char* prefix);

char* log_get_early_prefix_thread();

void log_unset_early_prefix_thread();

const char* log_level_to_string(
        log_level_t level);

time_t log_message_timestamp();

char* log_message_timestamp_str(
        time_t timestamp,
        char *dest,
        size_t maxlen);

void log_message_internal(
        const char *tag,
        log_level_t level,
        const char *message,
        va_list args);

void log_message(
        const char *tag,
        log_level_t level,
        const char *message,
        ...);

void log_message_print_os_error(
        const char *tag);

char* log_buffer_static_or_alloc_new(
        char* static_buffer,
        size_t static_buffer_size,
        size_t data_size,
        bool* static_buffer_selected);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_LOG_H
