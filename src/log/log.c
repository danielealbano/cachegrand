#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <locale.h>
#include <errno.h>
#include <string.h>

#include "misc.h"
#include "log.h"
#include "xalloc.h"

// TODO: Currently all the formatting and printing logic is embedded in log_message_internal_printer and a sort of
//       simple sink wrapper to pass around the FILE* where to print.
//       The current producers overlay managed via the LOG_PRODUCER_* macros / log_producer_* functions should go
//       because it doesn't provide anything and it's just a wrapper to the textual TAG so it should just be reverted to
//       be a simple textual tag.
//       Everytime a thread tries to submit a message to be logged should check if it has been registered with the
//       logging system and if not it should register, threads are not spawn dynamically so the logging thread will
//       waste away checks on ended threads, but if it will be necessary a GC can be performed on threads that are not
//       logging and the logging mechanism will take care of re-registering the thread if it has been de-registered
//       simply tracking the registration status.
//       The registration mechanism must be provided in a transparent way by the logging mechanism.
//       To make the logging thread safe and fast enough to cope with the performance requirements, a double ring-buffer
//       of pre-allocated objects should be used.

thread_local char* log_early_prefix_thread = NULL;

static log_sink_t log_sinks_registered_list[LOG_SINK_REGISTERED_MAX] = {0};
static uint8_t log_sinks_registered_count = 0;

static log_sink_t *log_sink_default_console;

FUNCTION_CTOR(log_sink_init_console, {
    log_sink_register(log_sink_console_init(LOG_LEVEL_ALL /*- LOG_LEVEL_DEBUG - LOG_LEVEL_VERBOSE*/));
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
        default:
            return "UNKNOWN";
    }
}

void log_set_early_prefix_thread(
        char* prefix) {
    log_early_prefix_thread = prefix;
}

char* log_get_early_prefix_thread() {
    return log_early_prefix_thread;
}

void log_unset_early_prefix_thread() {
    log_early_prefix_thread = NULL;
}

time_t log_message_timestamp() {
    return time(NULL);
}

char* log_message_timestamp_str(
        time_t timestamp,
        char* dest,
        size_t maxlen) {
    struct tm tm = { 0 };
    gmtime_r(&timestamp, &tm);

    snprintf(dest, maxlen, "%04d-%02d-%02dT%02d:%02d:%02dZ",
             1900 + tm.tm_year, tm.tm_mon, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);

    return dest;
}

void log_message_internal(
        const char *tag,
        log_level_t level,
        const char *message,
        va_list args) {
    time_t timestamp = log_message_timestamp();

    for(uint8_t i = 0; i < log_sinks_registered_count && i < LOG_SINK_REGISTERED_MAX; ++i){
        if ((level & log_sinks_registered_list[i].levels) != level) {
            continue;
        }

        log_sinks_registered_list[i].printer_fn(
                &log_sinks_registered_list[i].settings,
                tag,
                timestamp,
                level,
                log_early_prefix_thread,
                message,
                args);
    }
}

void log_message(
        const char *tag,
        log_level_t level,
        const char* message,
        ...) {
    va_list args;
    va_start(args, message);

    log_message_internal(tag, level, message, args);

    va_end(args);
}

void log_message_print_os_error(
        const char *tag) {
    int error_code;
#if defined(__linux__) || defined(__APPLE__)
    char buf[1024] = {0};
    char *error_message;
    error_code = errno;

    // If error code is OK skip
    if (error_code == 0) {
        return;
    }

    strerror_r(error_code, buf, sizeof(buf));
    error_message = buf;
#elif defined(__MINGW32__)
    DWORD last_error = GetLastError();
    error_code = last_error;
    LPVOID error_message;

    FormatMessage(
            (DWORD)FORMAT_MESSAGE_ALLOCATE_BUFFER |
            (DWORD)FORMAT_MESSAGE_FROM_SYSTEM |
            (DWORD)FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            last_error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &error_message,
            0,
            NULL
            );
#else
#error Platform not supported
#endif

    log_message(tag, LOG_LEVEL_ERROR, "OS Error: %s (%d)", error_message, error_code);

#if defined(__MINGW32__)
    LocalFree(error_message);
#endif
}

void log_sink_register(
        log_sink_t *sink) {
    log_sinks_registered_list[log_sinks_registered_count] = *sink;
    log_sinks_registered_count++;
}

log_sink_t *log_sink_init(
        log_sink_type_t type,
        FILE *out,
        log_level_t levels) {
    log_sink_t* result = xalloc_alloc_zero(sizeof(log_sink_t));

    result->type = type;
    result->out = out;
    result->levels = levels;

    return result;
}

void log_sink_free(
        log_sink_t* log_sink) {
    xalloc_free(log_sink);
}
