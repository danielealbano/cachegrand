/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>

#if DEBUG == 1
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif

#include "memory_fences.h"
#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "log/log.h"
#include "fatal.h"

#define TAG "spinlock"

void spinlock_init(
        spinlock_lock_volatile_t* spinlock) {
    spinlock->lock = SPINLOCK_UNLOCKED;
}

void spinlock_unlock(
        spinlock_lock_volatile_t* spinlock) {
#if DEBUG == 1
    long thread_id = syscall(__NR_gettid);
    uint32_t expected_lock = (uint32_t)thread_id;
    assert(spinlock->lock == expected_lock);
#endif

    spinlock->lock = SPINLOCK_UNLOCKED;
    MEMORY_FENCE_STORE();
}

bool spinlock_is_locked(
        spinlock_lock_volatile_t *spinlock) {
    MEMORY_FENCE_LOAD();
    return spinlock->lock != SPINLOCK_UNLOCKED;
}

bool spinlock_try_lock(
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

bool spinlock_lock_internal(
        spinlock_lock_volatile_t *spinlock,
        bool retry,
        const char* src_path,
        const char* src_func,
        uint32_t src_line) {
    bool res = false;
    uint32_t max_spins_before_probably_stuck = UINT32_MAX >> 1;

    while (unlikely(!(res = spinlock_try_lock(spinlock)) && retry)) {
        uint64_t spins = 0;
        while (likely(spinlock_is_locked(spinlock) && spins < max_spins_before_probably_stuck)) {
            spins++;
        }

        if (spins == max_spins_before_probably_stuck) {
            LOG_E(TAG, "Possible stuck spinlock detected for thread %lu in %s at %s:%u",
                    pthread_self(), src_func, src_path, src_line);
        }
    }

    return res;
}
