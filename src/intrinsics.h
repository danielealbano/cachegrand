#ifndef CACHEGRAND_INTRINSICS_H
#define CACHEGRAND_INTRINSICS_H

#ifdef __cplusplus
extern "C" {
#endif

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
    return (uint64_t) tsc;
#else
#error "unsupported platform"
#endif
}

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_INTRINSICS_H
