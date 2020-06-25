#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>

#include "memory_fences.h"
#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "log.h"
#include "fatal.h"

static log_producer_t* spinlock_log_producer;

static void __attribute__((constructor)) init_spinlock_log(){
    spinlock_log_producer = init_log_producer("spinlock");
}

static void __attribute__((constructor)) deinit_spinlock_log(){
    free(spinlock_log_producer);
}

void spinlock_init(
        spinlock_lock_volatile_t* spinlock) {
    spinlock->lock = SPINLOCK_UNLOCKED;
#if DEBUG == 1
    spinlock->magic = SPINLOCK_MAGIC;
#endif
    spinlock->predicted_spins = 0;
}

void spinlock_unlock(
        spinlock_lock_volatile_t* spinlock) {
    spinlock->lock = SPINLOCK_UNLOCKED;
    HASHTABLE_MEMORY_FENCE_STORE();
}

bool spinlock_is_locked(
        spinlock_lock_volatile_t *spinlock)
{
    HASHTABLE_MEMORY_FENCE_LOAD();
    return spinlock->lock == SPINLOCK_LOCKED;
}

bool spinlock_try_lock(
        spinlock_lock_volatile_t *spinlock)
{

#if CACHEGRAND_USE_LOCK_XCHGB == 1
    char prev;

    // Not really needed to specify the lock prefix here, the register is getting swapped with a memory location and
    // therefore, by specs, the lock prefix is implicit but better safe than sorry.
    __asm__ __volatile__(
        "lock xchgb %b0,%1"
        :"=q" (prev), "=m" (spinlock->lock)
        :"0" (SPINLOCK_LOCKED) : "memory");

    return prev != SPINLOCK_LOCKED;
#else
    uint8_t expected_value = SPINLOCK_UNLOCKED;
    uint8_t new_value = SPINLOCK_LOCKED;

    return __sync_bool_compare_and_swap(&spinlock->lock, expected_value, new_value);
#endif
}

bool spinlock_lock_internal(
        spinlock_lock_volatile_t *spinlock,
        bool retry,
        const char* src_path,
        const char* src_func,
        uint32_t src_line)
{
    bool res = false;

    while (unlikely(!(res = spinlock_try_lock(spinlock)) && retry)) {
        uint64_t spins = 0;
        while (unlikely(spinlock_is_locked(spinlock) && spins < UINT32_MAX)) {
            spins++;
        }

        // TODO: implement spinlock auto balancing using the predicted_spins property of the lock struct

        if (spins == UINT32_MAX) {
            LOG_E(spinlock_log_producer, "Possible stuck spinlock detected for thread %d in %s at %s:%u",
                    pthread_self(), src_func, src_path, src_line);
        }
    }

    return res;
}
