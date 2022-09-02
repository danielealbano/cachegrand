#ifndef CACHEGRAND_SPINLOCK_H
#define CACHEGRAND_SPINLOCK_H

#ifdef __cplusplus
#include <atomic>
#else
#include <stdatomic.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

#if DEBUG == 1
#include <assert.h>
#include <sys/types.h>
#endif

#include "cmake_config.h"
#include "misc.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "log/log.h"

#define SPINLOCK_UNLOCKED   0
#define SPINLOCK_LOCKED     1

typedef struct spinlock_lock spinlock_lock_t;
typedef _Volatile(spinlock_lock_t) spinlock_lock_volatile_t;
struct spinlock_lock {
    uint32_volatile_t lock;
} __attribute__((aligned(4)));

void spinlock_init(
        spinlock_lock_volatile_t* spinlock);

static inline __attribute__((always_inline)) void spinlock_unlock(
        spinlock_lock_volatile_t* spinlock) {
#if DEBUG == 1
    long thread_id = syscall(__NR_gettid);
    uint32_t expected_lock = (uint32_t)thread_id;
    assert(spinlock->lock == expected_lock);
#endif

    spinlock->lock = SPINLOCK_UNLOCKED;
    MEMORY_FENCE_STORE();
}

static inline __attribute__((always_inline)) bool spinlock_is_locked(
        spinlock_lock_volatile_t *spinlock) {
    MEMORY_FENCE_LOAD();
    return spinlock->lock != SPINLOCK_UNLOCKED;
}

static inline __attribute__((always_inline)) bool spinlock_try_lock(
        spinlock_lock_volatile_t *spinlock) {
#if DEBUG == 1
    long thread_id = syscall(__NR_gettid);
    uint32_t new_value = thread_id;
    uint32_t expected_value = SPINLOCK_UNLOCKED;
#else
    uint32_t new_value = SPINLOCK_LOCKED;
    uint32_t expected_value = SPINLOCK_UNLOCKED;
#endif

    return __sync_bool_compare_and_swap(&spinlock->lock, expected_value, new_value);
}

static inline __attribute__((always_inline)) void spinlock_lock_internal(
        spinlock_lock_volatile_t *spinlock,
        const char* src_path,
        uint32_t src_line) {
    uint32_t max_spins_before_probably_stuck = 1 << 23;

    uint64_t spins = 0;
    while (unlikely(!spinlock_try_lock(spinlock))) {
        if (unlikely(spins++ == max_spins_before_probably_stuck)) {
            LOG_E("spinlock", "Possible stuck spinlock detected for thread %lu in %s:%u",
                  syscall(__NR_gettid), src_path, src_line);
            spins = 0;
        }
    }
}

/**
 * Uses a macro to wrap spinlock_lock_internal to automatically define the path and the line args
 */
#define spinlock_lock(spinlock) \
    spinlock_lock_internal(spinlock, CACHEGRAND_SRC_PATH, __LINE__)

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_SPINLOCK_H
