#ifndef CACHEGRAND_MISC_H
#define CACHEGRAND_MISC_H

#ifdef __cplusplus
extern "C" {
#endif

uint64_t pow2_next_pow2m1(uint64_t x);
uint64_t pow2_next(uint64_t x);
bool pow2_is(uint64_t x);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MISC_H
