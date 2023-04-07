/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <unistd.h>

#include "misc.h"

#include "intrinsics.h"

uint64_t intrinsics_cycles_per_second = 0;

FUNCTION_CTOR(intrinsics_cycles_per_second_init, {
    intrinsics_cycles_per_second_calibrate();
})

void intrinsics_cycles_per_second_calibrate() {
    intrinsics_cycles_per_second = intrinsics_cycles_per_second_calculate();
}

uint64_t intrinsics_cycles_per_second_calculate() {
    uint64_t loops = 3;
    uint64_t loop_wait_ms = 300;
    uint64_t cycles_per_second_sum = 0;

    // Calculate the average cycles per second
    for(int i = 0; i < loops; i++) {
        uint64_t start = intrinsics_tsc();
        usleep(loop_wait_ms * 1000);
        cycles_per_second_sum += intrinsics_tsc() - start;
    }

    // Calculate the average cycles per second
    uint64_t cycles_per_second_internal =
            (uint64_t)(((double)cycles_per_second_sum / (double)loops) * (1000.0 / (double)loop_wait_ms));

    // Modern CPUs usually have GHz frequencies with 1 digit and one decimal, for example 42007283080 Hz is actually
    // 4.2 GHz (or 4200 MHz) so we need to divide by 10000000 and then multiply it back to get the correct value
    cycles_per_second_internal = (uint64_t)((uint64_t)cycles_per_second_internal / 10000000) * 10000000;

    return cycles_per_second_internal;
}
