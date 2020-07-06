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

#define LOG_E_OS_ERROR(producer) \
    log_message_print_os_error(producer);
#define LOG_E(producer, ...) \
    log_message(producer, LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_R(producer, ...) \
    log_message(producer, LOG_LEVEL_RECOVERABLE, __VA_ARGS__)
#define LOG_W(producer, ...) \
    log_message(producer, LOG_LEVEL_WARNING, __VA_ARGS__)
#define LOG_I(producer, ...) \
    log_message(producer, LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_V(producer, ...) \
    log_message(producer, LOG_LEVEL_VERBOSE, __VA_ARGS__)
#define LOG_D(producer, ...) \
    log_message(producer, LOG_LEVEL_DEBUG, __VA_ARGS__)

#define LOG_PRODUCER_DEFAULT PRODUCER

#define LOG_PRODUCER_CREATE_LOCAL(TAG, SUFFIX, VAR) \
    static log_producer_t* VAR; \
    FUNCTION_CTOR(concat(log_producer_local_init, SUFFIX), { \
        VAR = log_producer_init(TAG); \
    }) \
    FUNCTION_DTOR(concat(log_producer_local_free, SUFFIX), { \
        log_producer_free(VAR); \
    })

#define LOG_PRODUCER_CREATE_LOCAL_DEFAULT(TAG, SUFFIX) \
    LOG_PRODUCER_CREATE_LOCAL(TAG, SUFFIX, LOG_PRODUCER_DEFAULT)

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

typedef struct {
    FILE* out;
    log_level_t levels;
} log_sink_t;

typedef struct{
    char* tag;
} log_producer_t;

log_producer_t* log_producer_init(char *tag);
log_producer_t* log_producer_free(log_producer_t *log_producer);
log_sink_t* log_sink_init(FILE* out, log_level_t levels);
log_sink_t* log_sink_free(log_sink_t *log_sink);
const char* log_level_to_string(log_level_t level);
char* log_message_timestamp(char *dest, size_t maxlen);
void log_message_internal_printer(const char *tag, log_level_t level, const char *message, va_list args, FILE *out);
void log_message_internal(log_producer_t *producer, log_level_t level, const char *message, va_list args);
void log_message(log_producer_t *producer, log_level_t level, const char *message, ...);
void log_message_print_os_error(log_producer_t* producer);
void log_sink_register(log_sink_t *sink);
#ifndef DEBUG
#define LOG_DI(...) /* Internal debug logs disabled */
#endif // DEBUG == 1

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_LOG_H
