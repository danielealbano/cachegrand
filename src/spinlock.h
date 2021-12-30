#ifndef CACHEGRAND_SPINLOCK_H
#define CACHEGRAND_SPINLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cmake_config.h"

#define SPINLOCK_UNLOCKED   0
#define SPINLOCK_LOCKED     1

#if DEBUG == 1
#define SPINLOCK_MAGIC      42
#endif

typedef struct spinlock_lock spinlock_lock_t;
typedef _Volatile(spinlock_lock_t) spinlock_lock_volatile_t;
struct spinlock_lock {
#if DEBUG == 1
    uint16_volatile_t lock;
#else
    uint8_volatile_t lock;
    uint8_t padding;
#endif
    uint16_t predicted_spins;
} __attribute__((aligned(4)));

void spinlock_init(
        spinlock_lock_volatile_t* spinlock);

bool spinlock_try_lock(
        spinlock_lock_volatile_t* spinlock);

bool spinlock_is_locked(
        spinlock_lock_volatile_t* spinlock);

bool spinlock_lock_internal(
        spinlock_lock_volatile_t* spinlock,
        bool retry,
        const char* src_path,
        const char* src_func,
        uint32_t src_line);

void spinlock_unlock(
        spinlock_lock_volatile_t* spinlock);

/**
 * Uses a macro to wrap _spinlock_lock to automatically define the path, func and line args
 */
#define spinlock_lock(spinlock, retry) \
    spinlock_lock_internal(spinlock, retry, CACHEGRAND_SRC_PATH, __func__, __LINE__)

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_SPINLOCK_H
