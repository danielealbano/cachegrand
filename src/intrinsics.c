/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#if defined(__x86_64__)
#include <cpuid.h>
#endif

#include "misc.h"

#include "intrinsics.h"

uint64_t intrinsics_frequency_max_internal = 0;

FUNCTION_CTOR(intrinsics_cycles_per_second_init, {
    intrinsics_frequency_max_internal = intrinsics_frequency_max();
})

#if defined(__aarch64__)
uint64_t intrinsics_frequency_max_calculate_aarch64_cntfrq_el0() {
    uint64_t frq;
    asm volatile(
            "mrs %0, cntfrq_el0"
            : "=r"(frq));

    // Adjust the scale for compatibility with the x86_64 implementation
    return (uint32_t)frq * 100;
}
#endif

#if defined(__x86_64__)
uint64_t intrinsics_frequency_calculate_max_x64_cpuid_level_16h() {
    uint32_t eax, ebx, ecx, edx;
    if (__get_cpuid_max(0, NULL) < 0x16) {
        return 0;
    }

    __cpuid_count(0x16, 0, eax, ebx, ecx, edx);

    if (eax == 0) {
        return 0;
    }

    uint64_t cycles = ebx;
    cycles *= 1000;

    return cycles;
}
#endif

uint64_t intrinsics_frequency_max_calculate_simple() {
    uint64_t hz_per_second;
    uint64_t mhz_per_second;

    // Calculate the amount of cycles in a second
    uint64_t start = intrinsics_tsc();
    sleep(1);
    uint64_t end = intrinsics_tsc();

    // Check for overflow
    if (end < start) {
        hz_per_second = ((UINT64_MAX - start) + end);
    } else {
        hz_per_second = (end - start);
    }

    // Calculate the mhz for rounding
    mhz_per_second = hz_per_second / 1000000;

    // Round to the nearest 50 MHz
    if ((mhz_per_second % 50) > 15) {
        mhz_per_second = ((mhz_per_second / 50) * 50) + 50;
    } else {
        mhz_per_second = ((mhz_per_second / 50) * 50);
    }

    // Convert back to hz
    hz_per_second = mhz_per_second * 1000000;

    return hz_per_second;
}

uint64_t intrinsics_frequency_max_calculate() {
    uint64_t return_value;

#if defined(__x86_64__)
    return_value = intrinsics_frequency_calculate_max_x64_cpuid_level_16h();
#elif defined(__aarch64__)
    return_value = intrinsics_frequency_max_calculate_aarch64_cntfrq_el0();
#else
    return_value = 0;
#endif

    // If there is no way to calculate the frequency leveraging specific instructions or registers fallback to a simple
    // calculation
    if (return_value == 0) {
        return_value = intrinsics_frequency_max_calculate_simple();
    }

    return return_value;
}

bool intrinsics_frequency_max_estimated() {
#if defined(__x86_64__)
    return intrinsics_frequency_calculate_max_x64_cpuid_level_16h() > 0 ? false : true;
#elif defined(__aarch64__)
    return false;
#else
#error "Unsupported architecture"
#endif
}
