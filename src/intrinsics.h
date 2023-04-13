#ifndef CACHEGRAND_INTRINSICS_H
#define CACHEGRAND_INTRINSICS_H

#include "misc.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint64_t intrinsics_frequency_max_internal;

#if defined(__x86_64__)
uint64_t intrinsics_frequency_max_calculate_x64_cpuid_level_16h();
#elif defined(__aarch64__)
uint64_t intrinsics_frequency_max_calculate_aarch64_cntfrq_el0();
#endif

uint64_t intrinsics_frequency_max_calculate_simple();

uint64_t intrinsics_frequency_max_calculate();

bool intrinsics_frequency_max_estimated();

static inline uint64_t intrinsics_frequency_max() {
    if (unlikely(intrinsics_frequency_max_internal == 0)) {
        intrinsics_frequency_max_internal = intrinsics_frequency_max_calculate();
    }

    return intrinsics_frequency_max_internal;
}

static inline uint64_t intrinsics_tsc() {
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

    // Adjust the scale for compatibility with the x86_64 implementation
    return (uint64_t)tsc * 100;
#else
#error "unsupported platform"
#endif
}

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_INTRINSICS_H
