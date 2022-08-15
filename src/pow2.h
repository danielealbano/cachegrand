#ifndef CACHEGRAND_POW2_H
#define CACHEGRAND_POW2_H

#ifdef __cplusplus
extern "C" {
#endif

// https://jameshfisher.com/2018/03/30/round-up-power-2/
static inline __attribute__((always_inline)) uint64_t pow2_next_pow2m1(
        uint64_t x) {
    x |= x>>1;
    x |= x>>2;
    x |= x>>4;
    x |= x>>8;
    x |= x>>16;
    x |= x>>32;

    return x;
}

static inline __attribute__((always_inline)) uint64_t pow2_next(
        uint64_t x) {
    return pow2_next_pow2m1(x-1)+1;
}

static inline __attribute__((always_inline)) bool pow2_is(
        uint64_t x) {
    return x && (!(x & (x-1)));
}

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_POW2_H
