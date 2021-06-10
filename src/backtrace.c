/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

#if defined(__linux__)
#include <execinfo.h>
#else
#error Platform not supported
#endif

#include "misc.h"
#include "log/log.h"

#include "backtrace.h"

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
