#ifndef CACHEGRAND_THREAD_H
#define CACHEGRAND_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

long thread_current_get_id();
uint32_t thread_current_set_affinity(
        uint32_t thread_index);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_THREAD_H
