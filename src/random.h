#ifndef CACHEGRAND_RANDOM_H
#define CACHEGRAND_RANDOM_H

#ifdef __cplusplus
extern "C" {
#endif

struct random_init_state {
    uint64_t s;
};
typedef struct random_init_state random_init_state_t;

struct random_state {
    uint64_t a, b;
};
typedef struct random_state random_state_t;

uint64_t random_init_internal_seed(
        random_init_state_t *state);

random_state_t random_init(
        uint64_t seed);

uint64_t random_generate();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_RANDOM_H
