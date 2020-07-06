#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

#if defined(__linux__) || defined(__APPLE__)
#include <execinfo.h>
#elif defined(__MINGW32__)
#include <windows.h>
#include <strsafe.h>
#else
#error Platform not supported
#endif

#include "log.h"
#include "fatal.h"
#include "backtrace.h"

void fatal_trigger_debugger() {
#if defined(__APPLE__)
    volatile int* a;
    a = 0;
    *a = 0;

    // Raise the SIGABRT anyway in the code, in case the above statements get's optimized away by the compiler
    raise(SIGABRT);
#elif defined(__linux__) || defined(__MINGW32__)
    raise(SIGABRT);
#else
#error Platform not supported
#endif
}

void fatal_log_message(log_producer_t* producer, const char* message, va_list args) {
    log_message(producer, LOG_LEVEL_ERROR, "A fatal error has been throw, unable to continue.");
    log_message(producer, LOG_LEVEL_ERROR, "Please, review the details below.");
    log_message_internal(producer, LOG_LEVEL_ERROR, message, args);
    log_message_print_os_error(producer);
}

void fatal(log_producer_t* producer, const char* message, ...) {
    va_list args;

    va_start(args, message);
    fatal_log_message(
        producer,
        message,
        args);
    va_end(args);

    backtrace_print();

    fatal_trigger_debugger();

    exit(-1);
}