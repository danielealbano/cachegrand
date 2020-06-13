#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <locale.h>

#include "log.h"

#define LOG_MESSAGE_OUTPUT stdout

/**
 * TODO:
 *
 * Implement a log producers, sink & formatters patterns
 */

static log_level_t _log_level = LOG_LEVEL_DEBUG;

const char* log_level_to_string(log_level_t level) {
    switch(level) {
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

char* log_message_timestamp(char* t_str) {
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);

    sprintf(t_str, "%04d-%02d-%02dT%02d:%02d:%02dZ",
            1900 + tm.tm_year, tm.tm_mon, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);

    return t_str;
}

void log_message_internal(const char* tag, log_level_t level, const char* message, va_list args) {
    char t_str[LOG_MESSAGE_TIMESTAMP_MAX_LENGTH] = {0};

    if (level > _log_level) {
        return;
    }

    fprintf(LOG_MESSAGE_OUTPUT,
            "[%s][%-11s][%s] ",
            log_message_timestamp(t_str),
            log_level_to_string(level),
            tag);
    vfprintf(LOG_MESSAGE_OUTPUT, message, args);
    fprintf(LOG_MESSAGE_OUTPUT, "\n");
    fflush(LOG_MESSAGE_OUTPUT);
}

void log_set_log_level(log_level_t level) {
    _log_level = level;
}

void log_message(const char* tag, log_level_t level, const char* message, ...) {
    va_list args;
    va_start(args, message);

    log_message_internal(tag, level, message, args);

    va_end(args);
}
