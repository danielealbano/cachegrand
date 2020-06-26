#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <locale.h>

#include "log.h"

/**
 * TODO:
 *
 * Implement a log producers, sink & formatters patterns
 */

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

void log_message_internal(const char* tag, log_level_t level, const char* message, va_list args, FILE* out) {
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
void log_message_vargs(log_producer_t *tag, log_level_t level, const char *message, va_list args) {
    for(int i =0; i<= log_service->size-1; ++i){
        log_sink_t* sink = log_service->sinks[i];
        if ((level & sink->levels) != level) {
            continue;
        }
        log_message_internal(tag->tag, level, message, args,sink->out);
    }
}
void log_message(log_producer_t* tag, log_level_t level, const char* message, ...) {
    va_list args;
    va_start(args, message);

    log_message_vargs(tag,level,message,args);

    va_end(args);
}

void __attribute__((destructor)) deinit_log_service() {
    free(log_service->sinks);
    free(log_service);
}

void __attribute__((constructor)) init_log_service() {
    log_service = (log_service_t*)malloc(sizeof(log_service));
    //init console out
    log_sink_t* console = init_log_sink(stdout, LOG_LEVEL_INFO);
    log_service->sinks = malloc(sizeof(log_sink_t));
    *log_service->sinks = console;
    log_service->size = 1;
}

void log_sink_register(log_sink_t *sink) {
    ++log_service->size;
    log_service->sinks = realloc(log_service->sinks, sizeof(log_sink_t)*log_service->size);
    *(log_service->sinks + log_service->size - 1) = sink;
}

log_sink_t *init_log_sink(FILE *out, log_level_t min_level) {
    log_sink_t* result = (log_sink_t*)malloc(sizeof(log_sink_t));
    result->out = out;
    result->levels = levels;
    return result;
}

log_producer_t *init_log_producer(char *tag) {
    log_producer_t* result = malloc(sizeof(log_producer_t));
    result->service = log_service;
    result->tag = tag;
    return result;
}
