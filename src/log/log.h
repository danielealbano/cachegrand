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

#define LOG_LEVEL_STR_UNKNOWN_INDEX           0
#define LOG_LEVEL_STR_UNKNOWN_TEXT            "UNKNOWN"
#define LOG_LEVEL_STR_DEBUG_INTERNALS_INDEX   1
#define LOG_LEVEL_STR_DEBUG_INTERNALS_TEXT    "DEBUGINT"
#define LOG_LEVEL_STR_DEBUG_INDEX             2
#define LOG_LEVEL_STR_DEBUG_TEXT              "DEBUG"
#define LOG_LEVEL_STR_VERBOSE_INDEX           3
#define LOG_LEVEL_STR_VERBOSE_TEXT            "VERBOSE"
#define LOG_LEVEL_STR_INFO_INDEX              4
#define LOG_LEVEL_STR_INFO_TEXT               "INFO"
#define LOG_LEVEL_STR_WARNING_INDEX           5
#define LOG_LEVEL_STR_WARNING_TEXT            "WARNING"
#define LOG_LEVEL_STR_ERROR_INDEX             6
#define LOG_LEVEL_STR_ERROR_TEXT              "ERROR"

extern char *log_levels_text[];
extern thread_local char *log_early_prefix_thread;

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

#define LOG_LEVEL_ALL ((log_level_t)(((LOG_LEVEL_MAX - 1) * 2) - 1))

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
        ...) __attribute__ ((format(printf, 3, 4)));

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
