#include <stdint.h>
#include <time.h>

#include "random.h"

static __thread random_state_t random_state = {0};

uint64_t random_init_internal_seed(random_init_state_t *state) {
    uint64_t result = state->s;

    state->s = result + 0x9E3779B97f4A7C15;
    result = (result ^ (result >> 30U)) * 0xBF58476D1CE4E5B9;
    result = (result ^ (result >> 27U)) * 0x94D049BB133111EB;
    return result ^ (result >> 31U);
}

random_state_t random_init(uint64_t seed) {
    random_init_state_t init_state = {seed};
    random_state_t result = {0};

    result.a = random_init_internal_seed(&init_state);

    random_state = result;

    return result;
}

uint64_t random_generate()
{
    struct timespec seed;
    uint64_t x;

    if (random_state.a == 0) {
        clock_gettime(CLOCK_REALTIME, &seed);

        random_init(seed.tv_nsec);
    }

    x = random_state.a;
    x ^= x << 13U;
    x ^= x >> 7U;
    x ^= x << 17U;
    return random_state.a = x;
}
