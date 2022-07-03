/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <unistd.h>

#include "utils_cpu.h"

int utils_cpu_count() {
    static int count = 0;

    if (count == 0) {
#if defined(__linux__)
        count = sysconf(_SC_NPROCESSORS_ONLN);
#else
#error Platform not supported
#endif
    }

    return count;
}

int utils_cpu_count_all() {
    static int count = 0;

#if defined(__linux__)
    if (count != 0) {
        return count;
    }

    count = sysconf(_SC_NPROCESSORS_CONF);
#else
#error Platform not supported
#endif

    return count;
}
