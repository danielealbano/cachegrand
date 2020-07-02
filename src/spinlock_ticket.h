#ifndef CACHEGRAND_SPINLOCK_TICKET_H
#define CACHEGRAND_SPINLOCK_TICKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cmake_config.h"

typedef uint16_volatile_t spinlock_ticket_number_t;
typedef _Volatile(spinlock_ticket_number_t) spinlock_ticket_number_volatile_t;

typedef struct spinlock_ticket_lock spinlock_ticket_lock_t;
typedef _Volatile(spinlock_ticket_lock_t) spinlock_ticket_lock_volatile_t;
struct spinlock_ticket_lock {
    spinlock_ticket_number_t available;
    spinlock_ticket_number_t serving;
} __attribute__((aligned(4)));

void spinlock_ticket_init(
        spinlock_ticket_lock_volatile_t *spinlock);

spinlock_ticket_number_t spinlock_ticket_lock_internal(
        spinlock_ticket_lock_volatile_t *spinlock,
        const char* src_path,
        const char* src_func,
        uint32_t src_line);

void spinlock_ticket_unlock(
        spinlock_ticket_lock_volatile_t *spinlock);

/**
 * Uses a macro to wrap _spinlock_ticket_lock to automatically define the path, func and line args
 */
#define spinlock_ticket_lock(spinlock) \
    spinlock_ticket_lock_internal(spinlock, CACHEGRAND_SRC_PATH, __func__, __LINE__)

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_SPINLOCK_TICKET_H
