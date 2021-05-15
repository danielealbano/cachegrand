#ifndef CACHEGRAND_MEMORY_FENCES_H
#define CACHEGRAND_MEMORY_FENCES_H

#ifdef __cplusplus
#include <atomic>

using namespace std;
extern "C" {
#endif

#define MEMORY_FENCE_LOAD() atomic_thread_fence(memory_order_acquire)
#define MEMORY_FENCE_STORE() atomic_thread_fence(memory_order_release)
#define MEMORY_FENCE_LOAD_STORE() atomic_thread_fence(memory_order_acq_rel)

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MEMORY_FENCES_H
