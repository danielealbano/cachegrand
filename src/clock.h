#ifndef CACHEGRAND_CLOCK_H
#define CACHEGRAND_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
#include <ctime>
#include <cstdint>
#include <cstdbool>
#include <cassert>
#else
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#endif

#include "misc.h"
#include "fatal.h"

typedef struct timespec timespec_t;

int64_t clock_monotonic_coarse_get_resolution_ms();
int64_t clock_realtime_coarse_get_resolution_ms();

static inline __attribute__((always_inline)) int64_t clock_timespec_to_int64_ms(
        timespec_t *timespec) {
    time_t s = timespec->tv_sec * 1000;
    long ms = timespec->tv_nsec / 1000000;

    return s + ms;
}

static inline __attribute__((always_inline)) void clock_monotonic(
        timespec_t *timespec) {
    if (unlikely(clock_gettime(CLOCK_MONOTONIC, timespec) < 0)) {
        FATAL("clock", "Unable to fetch the time");
    }
}

static inline __attribute__((always_inline)) int64_t clock_monotonic_int64_ms() {
    timespec_t timespec;
    clock_monotonic(&timespec);
    return clock_timespec_to_int64_ms(&timespec);
}

static inline __attribute__((always_inline)) void clock_monotonic_coarse(
        timespec_t *timespec) {
    if (unlikely(clock_gettime(CLOCK_MONOTONIC_COARSE, timespec) < 0)) {
        FATAL("clock", "Unable to fetch the time");
    }
}

static inline __attribute__((always_inline)) int64_t clock_monotonic_coarse_int64_ms() {
    timespec_t timespec;
    clock_monotonic_coarse(&timespec);
    return clock_timespec_to_int64_ms(&timespec);
}

static inline __attribute__((always_inline)) void clock_realtime(
        timespec_t *timespec) {
    if (unlikely(clock_gettime(CLOCK_REALTIME, timespec) < 0)) {
        FATAL("clock", "Unable to fetch the time");
    }
}

static inline __attribute__((always_inline)) int64_t clock_realtime_int64_ms() {
    timespec_t timespec;
    clock_realtime(&timespec);
    return clock_timespec_to_int64_ms(&timespec);
}

static inline __attribute__((always_inline)) void clock_realtime_coarse(
        timespec_t *timespec) {
    if (unlikely(clock_gettime(CLOCK_REALTIME_COARSE, timespec) < 0)) {
        FATAL("clock", "Unable to fetch the time");
    }
}

static inline __attribute__((always_inline)) int64_t clock_realtime_coarse_int64_ms() {
    timespec_t timespec;
    clock_realtime_coarse(&timespec);
    return clock_timespec_to_int64_ms(&timespec);
}

static inline __attribute__((always_inline)) void clock_diff(
        timespec_t *a,
        timespec_t *b,
        timespec_t *result) {
    int64_t tv_nsec;
    int64_t tv_sec = a->tv_sec - b->tv_sec;

    if (unlikely(tv_sec < 0)) {
        tv_sec = tv_sec * -1;
        tv_nsec = b->tv_nsec - a->tv_nsec;
    } else {
        tv_nsec = a->tv_nsec - b->tv_nsec;
    }
    
    if (unlikely(tv_nsec < 0)) {
        tv_sec--;
        tv_nsec += (int64_t)1000000000;
    }

    result->tv_sec = tv_sec;
    result->tv_nsec = tv_nsec;
}

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_CLOCK_H
