#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <execinfo.h>

#include "log.h"
#include "fatal.h"

void fatal_trigger_debugger() {
#if defined(__APPLE__)
    volatile int* a;
    a = 0;
    *a = 0;

    // Raise the SIGABRT anyway in the code, in case the above statements get's optimized away by the compiler
    raise(SIGABRT);
#elif defined(__linux__)
    raise(SIGABRT);
#else
#error Platform not supported
#endif
}

void fatal_log_message(const char* tag, const char* message, va_list args) {
    char buf[1024];
    char *error_message;

    error_message = (char*)(uintptr_t)strerror_r(errno, buf, sizeof(buf));

    log_message(tag, LOG_LEVEL_ERROR, "A fatal error has been throw, unable to continue.");
    log_message(tag, LOG_LEVEL_ERROR, "Please, review the details below.");
    log_message_internal(tag, LOG_LEVEL_ERROR, message, args);
    log_message(tag, LOG_LEVEL_ERROR, "OS Error: %s (%d)", error_message, errno);
}

void fatal(const char* tag, const char* message, ...) {
    va_list args;
    va_start(args, message);
    fatal_log_message(tag, message, args);
    va_end(args);

    fatal_trigger_debugger();

    exit(-1);
}