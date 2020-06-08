#ifndef CACHEGRAND_MEMORY_FENCES_H
#define CACHEGRAND_MEMORY_FENCES_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASHTABLE_MEMORY_FENCE_LOAD() atomic_thread_fence(memory_order_acquire)
#define HASHTABLE_MEMORY_FENCE_STORE() atomic_thread_fence(memory_order_release)
#define HASHTABLE_MEMORY_FENCE_LOAD_STORE() atomic_thread_fence(memory_order_acq_rel)

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MEMORY_FENCES_H
