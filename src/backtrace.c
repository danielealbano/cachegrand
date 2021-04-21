#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

#if defined(__linux__)
#include <execinfo.h>
#else
#error Platform not supported
#endif

#include "backtrace.h"
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
