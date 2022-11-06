/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>

#include "intrinsics.h"

uint64_t intrinsic_tsc() {
#if defined(__x86_64__)
    uint64_t rax, rdx;
    __asm__ __volatile__ (
            "rdtsc"
            : "=a" (rax), "=d" (rdx));
    return (rdx << 32) + rax;
#elif defined(__aarch64__)
    int64_t tsc;
    asm volatile (
            "mrs %0, cntvct_el0"
            : "=r"(tsc));
    return (uint64_t) tsc;
#else
#error "unsupported platform"
#endif
}
