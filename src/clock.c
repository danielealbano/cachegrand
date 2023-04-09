/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "misc.h"
#include "intrinsics.h"

#include "clock.h"

int64_t clock_realtime_coarse_get_resolution_ms() {
    timespec_t res;
    clock_getres(CLOCK_REALTIME_COARSE, &res);
    int64_t res_ms = clock_timespec_to_int64_ms(&res);

    return res_ms;
}

int64_t clock_monotonic_coarse_get_resolution_ms() {
    return 1;
}

char *clock_timespan_human_readable(
        uint64_t timespan_ms,
        char *buffer,
        size_t buffer_length) {
    assert(buffer_length >= CLOCK_TIMESPAN_MIN_LENGTH);

    size_t offset = 0;
    uint64_t days = timespan_ms / (1000 * 60 * 60 * 24);
    uint64_t hours = (timespan_ms / (1000 * 60 * 60)) % 24;
    uint64_t minutes = (timespan_ms / (1000 * 60)) % 60;
    uint64_t seconds = (timespan_ms / 1000) % 60;

    if (days > 0) {
        offset += snprintf(buffer + offset, buffer_length - offset, "%lu days", days);
    }

    if (hours > 0) {
        offset += snprintf(buffer + offset, buffer_length - offset, "%s%lu hours", offset > 0 ? " " : "", hours);
    }

    if (minutes > 0) {
        offset += snprintf(buffer + offset, buffer_length - offset, "%s%lu minutes", offset > 0 ? " " : "", minutes);
    }

    if (seconds > 0) {
        offset += snprintf(buffer + offset, buffer_length - offset, "%s%lu seconds", offset > 0 ? " " : "", seconds);
    }

    if (offset == 0) {
        snprintf(buffer + offset, buffer_length - offset, "0 seconds");
    }

    return buffer;
}
