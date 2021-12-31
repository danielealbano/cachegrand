/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
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
    spinlock->predicted_spins = 0;
}

void spinlock_unlock(
        spinlock_lock_volatile_t* spinlock) {
#if DEBUG == 1
    long thread_id = syscall(__NR_gettid);
    uint8_t expected_lock = (uint8_t)thread_id;
    assert(spinlock->lock == expected_lock);
#endif

    spinlock->flags = 0;
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
    uint8_t new_value = thread_id;
    uint8_t expected_value = SPINLOCK_UNLOCKED;
#else
    uint8_t new_value = SPINLOCK_LOCKED;
    uint8_t expected_value = SPINLOCK_UNLOCKED;
#endif

    return __sync_bool_compare_and_swap(&spinlock->lock, expected_value, new_value);
}

void spinlock_set_flag(
        spinlock_lock_volatile_t *spinlock,
        spinlock_flag_t flag) {
    spinlock_flag_t old_flags, new_flags;
    old_flags = spinlock->flags;
    do {
        new_flags = old_flags | flag;
    } while(__sync_val_compare_and_swap(&spinlock->flags, old_flags, new_flags) != old_flags);
}

bool spinlock_unset_flag(
        spinlock_lock_volatile_t *spinlock,
        spinlock_flag_t flag) {
    spinlock_flag_t old_flags, new_flags;
    old_flags = spinlock->flags;
    do {
        new_flags = old_flags & ~flag;
    } while(__sync_val_compare_and_swap(&spinlock->flags, old_flags, new_flags) != old_flags);
}

bool spinlock_has_flag(
        spinlock_lock_volatile_t *spinlock,
        spinlock_flag_t flag) {
    MEMORY_FENCE_LOAD();
    spinlock_flag_t flags = spinlock->flags;

    return (flags & flag) == flag;
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

        // TODO: implement spinlock auto balancing using the predicted_spins property of the lock struct

        if (spins == max_spins_before_probably_stuck) {
            spinlock_set_flag(spinlock, SPINLOCK_FLAG_POTENTIALLY_STUCK);

            LOG_E(TAG, "Possible stuck spinlock detected for thread %lu in %s at %s:%u",
                    pthread_self(), src_func, src_path, src_line);
        }
    }

    return res;
}
