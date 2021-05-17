#ifndef CACHEGRAND_CLOCK_H
#define CACHEGRAND_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct timespec timespec_t;

void clock_monotonic(timespec_t *timespec);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_CLOCK_H
