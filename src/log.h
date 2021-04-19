#ifndef CACHEGRAND_LOG_H
#define CACHEGRAND_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <log_debug.h>
#include <stdio.h>

#define LOG_SINK_REGISTERED_MAX             4
#define LOG_MESSAGE_TIMESTAMP_MAX_LENGTH    50

#define LOG_E_OS_ERROR(tag) \
    log_message_print_os_error(tag);
#define LOG_E(tag, ...) \
    log_message(tag, LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_R(tag, ...) \
    log_message(tag, LOG_LEVEL_RECOVERABLE, __VA_ARGS__)
#define LOG_W(tag, ...) \
    log_message(tag, LOG_LEVEL_WARNING, __VA_ARGS__)
#define LOG_I(tag, ...) \
    log_message(tag, LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_V(tag, ...) \
    log_message(tag, LOG_LEVEL_VERBOSE, __VA_ARGS__)
#define LOG_D(tag, ...) \
    log_message(tag, LOG_LEVEL_DEBUG, __VA_ARGS__)

enum log_level {
    LOG_LEVEL_ERROR = 0x40,
    LOG_LEVEL_RECOVERABLE = 0x20,
    LOG_LEVEL_WARNING = 0x10,
    LOG_LEVEL_INFO = 0x08,
    LOG_LEVEL_VERBOSE = 0x04,
    LOG_LEVEL_DEBUG = 0x02,
    LOG_LEVEL_DEBUG_INTERNALS = 0x01,
};
typedef enum log_level log_level_t;

#define LOG_LEVEL_ALL ((uint8_t)LOG_LEVEL_ERROR << 1u) - 1u

enum log_sink_type {
    LOG_SINK_TYPE_CONSOLE = 0,
    LOG_SINK_TYPE_FILE
};
typedef enum log_sink_type log_sink_type_t;

typedef struct log_sink log_sink_t;
struct log_sink {
    log_sink_type_t type;
    FILE* out;
    log_level_t levels;
};

typedef struct log_producer log_producer_t;
struct log_producer {
    const char* tag;
};

void log_producer_set_early_prefix_thread(
        char* prefix);
char* log_producer_get_early_prefix_thread();
void log_producer_unset_early_prefix_thread();
log_sink_t* log_sink_init(
        log_sink_type_t type,
        FILE* out,
        log_level_t levels);
void log_sink_free(
        log_sink_t *log_sink);
const char* log_level_to_string(
        log_level_t level);
time_t log_message_timestamp();
char* log_message_timestamp_str(
        time_t timestamp,
        char *dest,
        size_t maxlen);
void log_message_internal_printer(
        const char *tag,
        log_level_t level,
        time_t timestamp,
        const char *message,
        va_list args,
        FILE *out);
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
void log_sink_register(
        log_sink_t *sink);
log_sink_t *log_sink_console_init(
        log_level_t levels);

#ifndef DEBUG
#define LOG_DI(...) /* Internal debug logs disabled */
#endif // DEBUG == 1

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_LOG_H
