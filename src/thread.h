#ifndef CACHEGRAND_THREAD_H
#define CACHEGRAND_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

uint32_t thread_current_set_affinity(
        int thread_index);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_THREAD_H
