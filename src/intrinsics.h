#ifndef CACHEGRAND_INTRINSICS_H
#define CACHEGRAND_INTRINSICS_H

#ifdef __cplusplus
extern "C" {
#endif

uint64_t intrinsic_rdtscp(
        uint32_t *aux);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_INTRINSICS_H
