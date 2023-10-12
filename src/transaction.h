#ifndef CACHEGRAND_TRANSACTION_H
#define CACHEGRAND_TRANSACTION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>

#define TRANSACTION_ID_NOT_ACQUIRED (0)
#define TRANSACTION_SPINLOCK_UNLOCKED   (0)

static uint32_t max_spins_before_probably_stuck = 1 << 26;

enum transaction_lock_type {
    TRANSACTION_LOCK_TYPE_NONE = 0,
    TRANSACTION_LOCK_TYPE_READ = 1,
    TRANSACTION_LOCK_TYPE_WRITE = 2,
};
typedef enum transaction_lock_type transaction_lock_type_t;

typedef struct transaction_id transaction_id_t;
typedef _Volatile(transaction_id_t) transaction_id_volatile_t;
struct transaction_id {
    union {
        struct {
            uint16_volatile_t transaction_index;
            uint16_volatile_t worker_index;
        };
        uint32_volatile_t id;
    };
};

typedef struct transaction_rwspinlock transaction_rwspinlock_t;
typedef _Volatile(transaction_rwspinlock_t) transaction_rwspinlock_volatile_t;
struct transaction_rwspinlock {
    union {
        uint64_volatile_t atomic_var;
        struct {
            uint32_volatile_t transaction_id;
            uint16_volatile_t readers_count;
            // not used, it's in place to be able to use an atomic operation to read together the transaction_d and the
            // readers_count
            uint16_volatile_t reserved;
        } internal_data;
    };
} __attribute__((aligned(8)));

typedef struct transaction_locks_list_entry transaction_locks_list_entry_t;
struct transaction_locks_list_entry {
    transaction_rwspinlock_volatile_t *spinlock;
    transaction_lock_type_t lock_type;
};

typedef struct transaction transaction_t;
struct transaction {
    transaction_id_volatile_t transaction_id;
    struct {
        uint32_t count;
        uint32_t size;
        transaction_locks_list_entry_t *list;
    } locks;
};

static inline __attribute__((always_inline)) void transaction_rwspinlock_init(
        transaction_rwspinlock_volatile_t* spinlock) {
    spinlock->internal_data.transaction_id = TRANSACTION_SPINLOCK_UNLOCKED;
    spinlock->internal_data.readers_count = 0;
}

static inline __attribute__((always_inline)) void transaction_rwspinlock_unlock_internal(
        transaction_rwspinlock_volatile_t* spinlock
#if DEBUG == 1
        ,transaction_t *transaction
#endif
) {

#if DEBUG == 1
    assert(spinlock->internal_data.transaction_id == transaction->transaction_id.id);
#endif

    spinlock->internal_data.transaction_id = TRANSACTION_SPINLOCK_UNLOCKED;
    MEMORY_FENCE_STORE();
}

static inline __attribute__((always_inline)) bool transaction_rwspinlock_is_write_locked(
        transaction_rwspinlock_volatile_t *spinlock) {
    MEMORY_FENCE_LOAD();
    return spinlock->internal_data.transaction_id != TRANSACTION_SPINLOCK_UNLOCKED;
}

static inline __attribute__((always_inline)) bool transaction_rwspinlock_is_owned_by_transaction(
        transaction_rwspinlock_volatile_t *spinlock,
        transaction_t *transaction) {
    MEMORY_FENCE_LOAD();
    return spinlock->internal_data.transaction_id == transaction->transaction_id.id;
}

static inline __attribute__((always_inline)) bool transaction_rwspinlock_try_write_lock_internal(
        transaction_rwspinlock_volatile_t *spinlock,
        transaction_t *transaction,
        uint16_t expected_readers) {
    assert(transaction->transaction_id.id != TRANSACTION_ID_NOT_ACQUIRED);

    transaction_rwspinlock_t new_value = { 0 };
    new_value.internal_data.transaction_id = transaction->transaction_id.id;
    new_value.internal_data.readers_count = expected_readers;

    // These 2 bytes must be always imported as they might not belong to the rwspinlock itself as by specs the
    // rwspinlock is only 6 bytes long.
    new_value.internal_data.reserved = spinlock->internal_data.reserved;

    transaction_rwspinlock_t expected_value = { 0 };
    expected_value.internal_data.transaction_id = TRANSACTION_SPINLOCK_UNLOCKED;
    expected_value.internal_data.readers_count = expected_readers;

    // These 2 bytes must be always imported as they might not belong to the rwspinlock itself as by specs the
    // rwspinlock is only 6 bytes long.
    expected_value.internal_data.reserved = spinlock->internal_data.reserved;

    bool res = __sync_bool_compare_and_swap(&spinlock->atomic_var, expected_value.atomic_var, new_value.atomic_var);

    return res;
}

static inline __attribute__((always_inline)) bool transaction_rwspinlock_wait_write_lock_internal(
        transaction_rwspinlock_volatile_t *spinlock,
        const char* src_path,
        uint32_t src_line) {
    uint64_t spins = 0;
    while (unlikely(transaction_rwspinlock_is_write_locked(spinlock))) {
        if (unlikely(spins++ == max_spins_before_probably_stuck)) {
            LOG_E("transaction_rwspinlock", "Possible transactional spinlock stuck detected for thread %lu in %s:%u",
                  syscall(__NR_gettid), src_path, src_line);
            return false;
        }
    }

    return true;
}

static inline __attribute__((always_inline)) bool transaction_rwspinlock_write_lock_internal(
        transaction_rwspinlock_volatile_t *spinlock,
        transaction_t *transaction,
        uint16_t expected_readers,
        const char* src_path,
        uint32_t src_line) {
    uint64_t spins = 0;
    while (unlikely(!transaction_rwspinlock_try_write_lock_internal(spinlock, transaction, expected_readers))) {
        if (unlikely(spins++ == max_spins_before_probably_stuck)) {
            LOG_E("transaction_rwspinlock", "Possible transactional spinlock stuck detected for thread %lu in %s:%u",
                  syscall(__NR_gettid), src_path, src_line);
            return false;
        }
    }

    return true;
}

static inline __attribute__((always_inline)) void transaction_rwspinlock_readers_increment(
        transaction_rwspinlock_volatile_t *spinlock) {
    assert(spinlock->internal_data.readers_count < UINT16_MAX);
    __sync_fetch_and_add(&spinlock->internal_data.readers_count, 1);
}

static inline __attribute__((always_inline)) void transaction_rwspinlock_readers_decrement(
        transaction_rwspinlock_volatile_t *spinlock) {
    assert(spinlock->internal_data.readers_count > 0);
    __sync_fetch_and_sub(&spinlock->internal_data.readers_count, 1);
}

void transaction_set_worker_index(
        uint32_t worker_index);

bool transaction_expand_locks_list(
        transaction_t *transaction);

uint16_t transaction_peek_current_thread_index();

bool transaction_acquire(
        transaction_t* transaction);

void transaction_release(
        transaction_t* transaction);

static inline __attribute__((always_inline)) bool transaction_needs_expand_locks_list(
        transaction_t* transaction) {
    return transaction->locks.count == transaction->locks.size;
}

static inline __attribute__((always_inline)) bool transaction_locks_list_add(
        transaction_t *transaction,
        transaction_rwspinlock_volatile_t *transaction_rwspinlock,
        transaction_lock_type_t lock_type) {
    assert(transaction != NULL);
    assert(transaction_rwspinlock != NULL);
    assert(lock_type != TRANSACTION_LOCK_TYPE_NONE);

    if (unlikely(transaction_needs_expand_locks_list(transaction))) {
        if (unlikely(!transaction_expand_locks_list(transaction))) {
            return false;
        }
    }

    transaction->locks.list[transaction->locks.count].spinlock = transaction_rwspinlock;
    transaction->locks.list[transaction->locks.count].lock_type = lock_type;
    transaction->locks.count++;

    return true;
}

static inline __attribute__((always_inline)) bool transaction_lock_for_write_internal(
        transaction_t *transaction,
        transaction_rwspinlock_volatile_t *transaction_rwspinlock,
        const char* src_path,
        uint32_t src_line) {

    if (likely(!transaction_rwspinlock_is_owned_by_transaction(
            transaction_rwspinlock,
            transaction))) {
        if (unlikely(!transaction_rwspinlock_write_lock_internal(
                transaction_rwspinlock,
                transaction,
                0,
                src_path,
                src_line))) {
            return false;
        }
    }

    if (!transaction_locks_list_add(
            transaction,
            transaction_rwspinlock,
            TRANSACTION_LOCK_TYPE_WRITE)) {
        // Unlock the spinlock and fail the operation
        transaction_rwspinlock_unlock_internal(
                transaction_rwspinlock
#if DEBUG == 1
                , transaction
#endif
        );
        return false;
    }

    return true;
}

static inline __attribute__((always_inline)) bool transaction_upgrade_lock_for_write_internal(
        transaction_t *transaction,
        transaction_rwspinlock_volatile_t *transaction_rwspinlock,
        const char* src_path,
        uint32_t src_line) {
    assert(transaction != NULL);
    assert(transaction_rwspinlock != NULL);

    if (likely(!transaction_rwspinlock_is_owned_by_transaction(
            transaction_rwspinlock,
            transaction))) {
        if (unlikely(!transaction_rwspinlock_write_lock_internal(
                transaction_rwspinlock,
                transaction,
                1,
                src_path,
                src_line))) {
            return false;
        }
    }

    if (!transaction_locks_list_add(
            transaction,
            transaction_rwspinlock,
            TRANSACTION_LOCK_TYPE_WRITE)) {
        // Unlock the spinlock and fail the operation
        transaction_rwspinlock_unlock_internal(
                transaction_rwspinlock
#if DEBUG == 1
                , transaction
#endif
        );

        return false;
    }

    return true;
}

static inline __attribute__((always_inline)) bool transaction_lock_for_read_internal(
        transaction_t *transaction,
        transaction_rwspinlock_volatile_t *transaction_rwspinlock,
        const char* src_path,
        uint32_t src_line) {
    assert(transaction != NULL);
    assert(transaction_rwspinlock != NULL);
    assert(transaction->transaction_id.id != transaction_rwspinlock->internal_data.transaction_id);

    // Increment the readers count
    transaction_rwspinlock_readers_increment(transaction_rwspinlock);

    // Ensure that there isn't a transaction in progress
    if (unlikely(!transaction_rwspinlock_wait_write_lock_internal(
            transaction_rwspinlock,
            src_path,
            src_line))) {
        goto cleanup;
    }

    if (!transaction_locks_list_add(
            transaction,
            transaction_rwspinlock,
            TRANSACTION_LOCK_TYPE_READ)) {
        goto cleanup;
    }

    return true;

cleanup:

    transaction_rwspinlock_readers_decrement(transaction_rwspinlock);
    return false;
}

/**
 * Uses a macro to wrap transaction_rwspinlock_write_lock_internal to automatically define the path and the line args
 */
#define transaction_lock_for_write(transaction_var, transaction_rwspinlock_var) \
    transaction_lock_for_write_internal(transaction_var, transaction_rwspinlock_var, CACHEGRAND_SRC_PATH, __LINE__)

/**
 * Uses a macro to wrap transaction_upgrade_lock_for_write_internal to automatically define the path and the line args
 */
#define transaction_upgrade_lock_for_write(transaction_var, transaction_rwspinlock_var) \
    transaction_upgrade_lock_for_write_internal(transaction_var, transaction_rwspinlock_var, CACHEGRAND_SRC_PATH, __LINE__)

/**
 * Uses a macro to wrap transaction_upgrade_lock_for_write_internal to automatically define the path and the line args
 */
#define transaction_lock_for_read(transaction_var, transaction_rwspinlock_var) \
    transaction_lock_for_read_internal(transaction_var, transaction_rwspinlock_var, CACHEGRAND_SRC_PATH, __LINE__)

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_TRANSACTION_H
