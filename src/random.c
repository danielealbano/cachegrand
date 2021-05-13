/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <time.h>

#include "random.h"

static __thread random_state_t random_state = {0};

uint64_t random_init_internal_seed(
        random_init_state_t *state) {
    uint64_t result = state->s;

    state->s = result + 0x9E3779B97f4A7C15;
    result = (result ^ (result >> 30U)) * 0xBF58476D1CE4E5B9;
    result = (result ^ (result >> 27U)) * 0x94D049BB133111EB;
    return result ^ (result >> 31U);
}

random_state_t random_init(
        uint64_t seed) {
    random_init_state_t init_state = {seed};
    random_state_t result = {0};

    result.a = random_init_internal_seed(&init_state);
    result.b = random_init_internal_seed(&init_state);

    random_state = result;

    return result;
}

uint64_t random_generate()
{
    uint64_t t, s;
    struct timespec seed;

    if (random_state.a == 0) {
        clock_gettime(CLOCK_REALTIME, &seed);

        random_init(seed.tv_nsec);
    }

    t = random_state.a;
    s = random_state.b;
    random_state.a = s;
    t ^= t << 23U;
    t ^= t >> 17U;
    t ^= s ^ (s >> 26U);
    random_state.b = t;

    return t + s;
}
