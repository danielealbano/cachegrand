#ifndef CACHEGRAND_TRANSACTION_SPINLOCK_H
#define CACHEGRAND_TRANSACTION_SPINLOCK_H

#ifdef __cplusplus
#include <atomic>
#else
#include <stdatomic.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <assert.h>

#include "cmake_config.h"
#include "misc.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "log/log.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"

#define TRANSACTION_ID_NOT_ACQUIRED (0)
#define TRANSACTION_SPINLOCK_UNLOCKED   (0)

typedef struct transaction transaction_t;

typedef struct transaction_spinlock_lock transaction_spinlock_lock_t;
typedef _Volatile(transaction_spinlock_lock_t) transaction_spinlock_lock_volatile_t;
struct transaction_spinlock_lock {
    uint32_volatile_t transaction_id;
} __attribute__((aligned(4)));

void transaction_spinlock_init(
        transaction_spinlock_lock_volatile_t* spinlock);

static inline __attribute__((always_inline)) void transaction_spinlock_unlock_internal(
        transaction_spinlock_lock_volatile_t* spinlock
#if DEBUG == 1
        ,transaction_t *transaction
#endif
    ) {

#if DEBUG == 1
    assert(spinlock->transaction_id == transaction->transaction_id.id);
#endif

    spinlock->transaction_id = TRANSACTION_SPINLOCK_UNLOCKED;
    MEMORY_FENCE_STORE();
}

static inline __attribute__((always_inline)) bool transaction_spinlock_is_locked(
        transaction_spinlock_lock_volatile_t *spinlock) {
    MEMORY_FENCE_LOAD();
    return spinlock->transaction_id != TRANSACTION_SPINLOCK_UNLOCKED;
}

static inline __attribute__((always_inline)) bool transaction_spinlock_is_owned_by_transaction(
        transaction_spinlock_lock_volatile_t *spinlock,
        transaction_t *transaction) {
    MEMORY_FENCE_LOAD();
    return spinlock->transaction_id == transaction->transaction_id.id;
}

static inline __attribute__((always_inline)) bool transaction_spinlock_try_lock(
        transaction_spinlock_lock_volatile_t *spinlock,
        transaction_t *transaction) {
    assert(transaction->transaction_id.id != TRANSACTION_ID_NOT_ACQUIRED);

    uint32_t new_value = transaction->transaction_id.id;
    uint32_t expected_value = TRANSACTION_SPINLOCK_UNLOCKED;

    bool res = __sync_bool_compare_and_swap(&spinlock->transaction_id, expected_value, new_value);

    if (likely(res)) {
        transaction_locks_list_add(transaction, spinlock);
    }

    return res;
}

static inline __attribute__((always_inline)) bool transaction_spinlock_lock_internal(
        transaction_spinlock_lock_volatile_t *spinlock,
        transaction_t *transaction,
        const char* src_path,
        uint32_t src_line) {
    uint32_t max_spins_before_probably_stuck = 1 << 26;

    uint64_t spins = 0;
    while (unlikely(!transaction_spinlock_try_lock(spinlock, transaction))) {
        if (unlikely(spins++ == max_spins_before_probably_stuck)) {
            LOG_E("transaction_spinlock", "Possible stuck transactional spinlock detected for thread %lu in %s:%u",
                  syscall(__NR_gettid), src_path, src_line);
            return false;
        }
    }

    return true;
}

/**
 * Uses a macro to wrap transaction_spinlock_lock_internal to automatically define the path and the line args
 */
#define transaction_spinlock_lock(transaction_spinlock_var, transaction) \
    transaction_spinlock_lock_internal(transaction_spinlock_var, transaction, CACHEGRAND_SRC_PATH, __LINE__)

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_TRANSACTION_SPINLOCK_H
