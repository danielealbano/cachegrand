/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#if defined(__linux__)
#include <execinfo.h>
#else
#error Platform not supported
#endif

#include "misc.h"
#include "log/log.h"

void backtrace_print() {
#if defined(__linux__)
    void *array[50];
    int size;

    size = backtrace(array, 50);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
#else
#error Platform not supported
#endif
}

void fatal_trigger_debugger() {
#if defined(__linux__)
    raise(SIGABRT);
#else
#error Platform not supported
#endif
}

void fatal_log_message(
        const char *tag,
        const char* message,
        va_list args) {
    log_message(tag, LOG_LEVEL_ERROR, "A fatal error has been throw, unable to continue.");
    log_message(tag, LOG_LEVEL_ERROR, "Please, review the details below.");
    log_message_internal(tag, LOG_LEVEL_ERROR, message, args);
    log_message_print_os_error(tag);
}

void fatal(
        const char *tag,
        const char* message, ...) {
    va_list args;

    va_start(args, message);
    fatal_log_message(
            tag,
            message,
            args);
    va_end(args);

    backtrace_print();

    fatal_trigger_debugger();

    exit(-1);
}
