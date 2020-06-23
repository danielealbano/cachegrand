#ifndef CACHEGRAND_LOG_H
#define CACHEGRAND_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <log_debug.h>
#include <stdio.h>

#define LOG_MESSAGE_TIMESTAMP_MAX_LENGTH 25

#define LOG_E(tag, message, ...) \
    log_message(tag, LOG_LEVEL_ERROR, message, __VA_ARGS__)
#define LOG_R(tag, message, ...) \
    log_message(tag, LOG_LEVEL_RECOVERABLE, message, __VA_ARGS__)
#define LOG_W(tag, message, ...) \
    log_message(tag, LOG_LEVEL_WARNING, message, __VA_ARGS__)
#define LOG_I(tag, message, ...) \
    log_message(tag, LOG_LEVEL_INFO, message, __VA_ARGS__)
#define LOG_V(tag, message, ...) \
    log_message(tag, LOG_LEVEL_VERBOSE, message, __VA_ARGS__)
#define LOG_D(tag, message, ...) \
    log_message(tag, LOG_LEVEL_DEBUG, message, __VA_ARGS__)

enum log_level {
    LOG_LEVEL_ERROR,
    LOG_LEVEL_RECOVERABLE,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO,
    LOG_LEVEL_VERBOSE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_DEBUG_INTERNALS,
};
typedef enum log_level log_level_t;

typedef struct {
    FILE* out;
    log_level_t min_level;
} log_sink_t;

typedef struct {
    log_sink_t** sinks;
    int size;
} log_service_t;

typedef struct{
    log_service_t* service;
    char* tag;
} log_producer_t;

log_producer_t* init_log_producer(char* tag);
log_sink_t* init_log_sink(FILE* out, log_level_t min_level);
const char* log_level_to_string(log_level_t level);
char* log_message_timestamp(char* dest, size_t maxlen);
void log_message_internal(const char* tag, log_level_t level, const char* message, va_list args, FILE* out);
void log_message(log_producer_t* tag, log_level_t level, const char* message, ...);

static log_service_t* log_service;

void __attribute__((constructor)) init_log_service();
void __attribute__((destructor)) deinit_log_service();

void register_sink(log_sink_t* sink);
#ifndef DEBUG
#define LOG_DI(...) /* Internal debug logs disabled */
#endif // DEBUG == 1

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_LOG_H
