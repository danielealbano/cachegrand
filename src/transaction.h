#ifndef CACHEGRAND_TRANSACTION_H
#define CACHEGRAND_TRANSACTION_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct transaction_spinlock_lock transaction_spinlock_lock_t;
typedef _Volatile(transaction_spinlock_lock_t) transaction_spinlock_lock_volatile_t;

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

typedef struct transaction transaction_t;
struct transaction {
    transaction_id_volatile_t transaction_id;
    struct {
        uint32_t count;
        uint32_t size;
        transaction_spinlock_lock_volatile_t **list;
    } locks;
};

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
        transaction_spinlock_lock_volatile_t *transaction_spinlock) {
    if (unlikely(transaction_needs_expand_locks_list(transaction))) {
        if (unlikely(!transaction_expand_locks_list(transaction))) {
            return false;
        }
    }

    transaction->locks.list[transaction->locks.count] = transaction_spinlock;
    transaction->locks.count++;

    return true;
}

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_TRANSACTION_H
