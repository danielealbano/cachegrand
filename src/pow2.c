/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>

// https://jameshfisher.com/2018/03/30/round-up-power-2/
uint64_t pow2_next_pow2m1(
        uint64_t x) {
    x |= x>>1;
    x |= x>>2;
    x |= x>>4;
    x |= x>>8;
    x |= x>>16;
    x |= x>>32;

    return x;
}

uint64_t pow2_next(
        uint64_t x) {
    return pow2_next_pow2m1(x-1)+1;
}

bool pow2_is(
        uint64_t x) {
    return x && (!(x & (x-1)));
}
