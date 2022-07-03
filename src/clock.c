/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <time.h>

#include "fatal.h"

#include "clock.h"

#define TAG "clock"

void clock_monotonic(timespec_t *timespec) {
    if (clock_gettime(CLOCK_MONOTONIC, timespec) < 0) {
        FATAL(TAG, "Unable to fetch the time");
    }
}

void clock_diff(timespec_t *result, timespec_t *a, timespec_t *b) {
    result->tv_sec = a->tv_sec - b->tv_sec;
    result->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (result->tv_nsec < 0) {
        result->tv_sec--;
        result->tv_nsec += 1000000000L;
    }
}
