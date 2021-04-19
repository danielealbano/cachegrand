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
//       To make the logging thread safe and flexible for each thread using the logging there should be a thread_local
//       ring buffer with a fixed size, the producing thread will be the only updating the entries and 2 counters will
//       be used to keep track of the enqueued and dequeued entries.
//       The entries will have to be dequeued by a log worker thread per sink, the ring buffers will have to allow each
//       worker thread to track the processed entries, this is not a big deal because we can preallocate a fixed number
//       of counters because we do allow only up to a certain amount of log sinks (currently this is defined via the
//       LOG_SINK_REGISTERED_MAX macro and the memory pre-allocated but it can be allocated at runtime at the start
//       before logging is being used, they just can't easily change during the execution).
//       When a thread starts to produce new logs, it has to register the log ring buffer with the log workers of the
//       sink but because this operation has to run within the producer thread it has to rely on atomic ops to safely
//       update the list of tracked ring buffers.
//       To enqueue and dequeue the ring buffer entries, simple memory barriers are more than enough.
//       If a producer will saturate the ring buffer it will just wait, although in a future an overflow list can be
//       maintained so the producer will be able to proceed and once there will be enough room it will copy the data
//       but also making it wait is not a bad idea beacuse will release some of the pressure from the worker and
//       provide a good balance.

thread_local char* log_producer_early_prefix_thread = NULL;

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

void log_producer_set_early_prefix_thread(
        char* prefix) {
    log_producer_early_prefix_thread = prefix;
}

char* log_producer_get_early_prefix_thread() {
    return log_producer_early_prefix_thread;
}

void log_producer_unset_early_prefix_thread() {
    log_producer_early_prefix_thread = NULL;
}

char* log_message_timestamp(
        char* dest,
        size_t maxlen) {
    struct tm tm = {0};
    time_t t = time(NULL);
    gmtime_r(&t, &tm);

    snprintf(dest, maxlen, "%04d-%02d-%02dT%02d:%02d:%02dZ",
            1900 + tm.tm_year, tm.tm_mon, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);

    return dest;
}

void log_message_internal_printer(
        const char* tag,
        log_level_t level,
        const char* message,
        va_list args,
        FILE* out) {
    char t_str[LOG_MESSAGE_TIMESTAMP_MAX_LENGTH] = {0};

    fprintf(out,
            "[%s][%-11s]%s[%s] ",
            log_message_timestamp(t_str, LOG_MESSAGE_TIMESTAMP_MAX_LENGTH),
            log_level_to_string(level),
            log_producer_early_prefix_thread != NULL ? log_producer_early_prefix_thread : "",
            tag);
    vfprintf(out, message, args);
    fprintf(out, "\n");
    fflush(out);
}

void log_message_internal(
        const char *tag,
        log_level_t level,
        const char *message,
        va_list args) {
    for(uint8_t i = 0; i < log_sinks_registered_count && i < LOG_SINK_REGISTERED_MAX; ++i){
        if ((level & log_sinks_registered_list[i].levels) != level) {
            continue;
        }

        log_message_internal_printer(
                tag,
                level,
                message,
                args,
                log_sinks_registered_list[i].out);
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

log_sink_t *log_sink_console_init(
        log_level_t levels) {
    return log_sink_init(
            LOG_SINK_TYPE_CONSOLE,
            stdout,
            levels);
}
