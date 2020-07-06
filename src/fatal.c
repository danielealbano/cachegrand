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

#if defined(__MINGW32__)
void fatal_log_message(const char* tag, DWORD last_error, const char* message, va_list args) {
#else
void fatal_log_message(log_producer_t* tag, const char* message, va_list args) {
#endif
#if defined(__linux__) || defined(__APPLE__)
    char buf[1024];
    char *error_message;
    error_message = (char*)(uintptr_t)strerror_r(errno, buf, sizeof(buf));
#elif defined(__MINGW32__)
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

    log_message(tag, LOG_LEVEL_ERROR, "A fatal error has been throw, unable to continue.");
    log_message(tag, LOG_LEVEL_ERROR, "Please, review the details below.");
    log_message_internal(tag, LOG_LEVEL_ERROR, message, args);
    log_message(tag, LOG_LEVEL_ERROR, "OS Error: %s (%d)", error_message, errno);

#if defined(__MINGW32__)
    LocalFree(error_message);
#endif
}

void fatal(log_producer_t* producer, const char* message, ...) {
    va_list args;

#if defined(__MINGW32__)
    DWORD last_error = GetLastError();
#endif

    va_start(args, message);
    fatal_log_message(
        tag,
#if defined(__MINGW32__)
#endif
        producer,
        message,
        args);
    va_end(args);

    backtrace_print();

    fatal_trigger_debugger();

    exit(-1);
}