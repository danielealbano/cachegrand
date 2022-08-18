/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include "clock.h"

int64_t clock_realtime_coarse_get_resolution_ms() {
    timespec_t res;
    clock_getres(CLOCK_REALTIME_COARSE, &res);
    int64_t res_ms = clock_timespec_to_int64_ms(&res);

    return res_ms;
}

int64_t clock_monotonic_coarse_get_resolution_ms() {
    timespec_t res;
    clock_getres(CLOCK_MONOTONIC_COARSE, &res);
    int64_t res_ms = clock_timespec_to_int64_ms(&res);

    return res_ms;
}
