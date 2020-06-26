#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <locale.h>

#include "misc.h"
#include "log.h"
#include "xalloc.h"

/**
 * TODO:
 *
 * Implement a log producers, sink & formatters patterns
 */
static log_sink_t log_sinks_registered_list[LOG_SINK_REGISTERED_MAX] = {0};
static uint8_t log_sinks_registered_count = 0;

static log_sink_t *log_sink_default_console;

FUNCTION_CTOR(log_sink_init_console, {
    log_sink_default_console = log_sink_init(stdout, LOG_LEVEL_ALL);
    log_sink_register(log_sink_default_console);
})

FUNCTION_DTOR(log_sink_free_console, {
    log_sink_free(log_sink_default_console);
})

const char* log_level_to_string(log_level_t level) {
    switch(level) {
        case LOG_LEVEL_DEBUG_INTERNALS:
            return "DEBUGINT";
        case LOG_LEVEL_DEBUG:
            return "DEBUG";
        case LOG_LEVEL_VERBOSE:
            return "VERBOSE";
        case LOG_LEVEL_INFO:
            return "INFO";
        case LOG_LEVEL_WARNING:
            return "WARNING";
        case LOG_LEVEL_RECOVERABLE:
            return "RECOVERABLE";
        case LOG_LEVEL_ERROR:
            return "ERROR";
    }
}

char* log_message_timestamp(char* dest, size_t maxlen) {
    struct tm tm = {0};
    time_t t = time(NULL);
    gmtime_r(&t, &tm);

    snprintf(dest, maxlen, "%04d-%02d-%02dT%02d:%02d:%02dZ",
            1900 + tm.tm_year, tm.tm_mon, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);

    return dest;
}

void log_message_internal_printer(const char* tag, log_level_t level, const char* message, va_list args, FILE* out) {
    char t_str[LOG_MESSAGE_TIMESTAMP_MAX_LENGTH] = {0};

    fprintf(out,
            "[%s][%-11s][%s] ",
            log_message_timestamp(t_str, LOG_MESSAGE_TIMESTAMP_MAX_LENGTH),
            log_level_to_string(level),
            tag);
    vfprintf(out, message, args);
    fprintf(out, "\n");
    fflush(out);
}

void log_message_internal(log_producer_t *producer, log_level_t level, const char *message, va_list args) {
    for(uint8_t i = 0; i < log_sinks_registered_count; ++i){
        log_sink_t* sink = &log_sinks_registered_list[i];
        if ((level & sink->levels) != level) {
            continue;
        }

        log_message_internal_printer(producer->tag, level, message, args,sink->out);
    }
}

void log_message(log_producer_t* producer, log_level_t level, const char* message, ...) {
    va_list args;
    va_start(args, message);

    log_message_internal(producer, level,message,args);

    va_end(args);
}

void log_sink_register(log_sink_t *sink) {
    log_sinks_registered_list[log_sinks_registered_count] = *sink;
    log_sinks_registered_count++;
}

log_sink_t *log_sink_init(FILE *out, log_level_t levels) {
    log_sink_t* result = (log_sink_t*)xalloc_alloc(sizeof(log_sink_t));
    result->out = out;
    result->levels = levels;
    return result;
}

log_sink_t* log_sink_free(log_sink_t* log_sink) {
    xalloc_free(log_sink);
}

log_producer_t *log_producer_init(char *tag) {
    log_producer_t* result = xalloc_alloc(sizeof(log_producer_t));
    result->tag = tag;
    return result;
}

log_producer_t* log_producer_free(log_producer_t* log_producer) {
    xalloc_free(log_producer);
}
