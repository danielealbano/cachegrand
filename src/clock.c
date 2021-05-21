/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
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
