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
#include <assert.h>

#include "memory_fences.h"
#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "fatal.h"
#include "spinlock_ticket.h"

#define TAG "spinlock_ticket"

static inline spinlock_ticket_number_t spinlock_ticket_acquire(
        spinlock_ticket_lock_volatile_t *spinlock_ticket)
{
    return __sync_fetch_and_add(&spinlock_ticket->available, 1);
}

static inline spinlock_ticket_number_t spinlock_ticket_serving(
        spinlock_ticket_lock_volatile_t *spinlock_ticket) {
    MEMORY_FENCE_LOAD();

    return spinlock_ticket->serving;
}

void spinlock_ticket_init(
        spinlock_ticket_lock_volatile_t *spinlock_ticket) {
    spinlock_ticket->available = 0;
    spinlock_ticket->serving = 0;
}

void spinlock_ticket_unlock(
        spinlock_ticket_lock_volatile_t *spinlock_ticket) {
    spinlock_ticket->serving++;
    MEMORY_FENCE_STORE();
}

spinlock_ticket_number_t spinlock_ticket_lock_internal(
        spinlock_ticket_lock_volatile_t *spinlock_ticket,
        const char* src_path,
        const char* src_func,
        uint32_t src_line)
{
    uint16_t my_ticket = spinlock_ticket_acquire(spinlock_ticket);

    uint64_t spins = 0;
    while (unlikely(spinlock_ticket_serving(spinlock_ticket) != my_ticket)) {
        for(uint32_t i = 0; i < (1u << 2u); i++) {
            asm("nop");
        }

        if (spins++ == UINT32_MAX) {
            LOG_E(TAG, "Possible stuck spinlock detected for thread %lu in %s at %s:%u",
                  pthread_self(), src_func, src_path, src_line);
        }
    }

    return my_ticket;
}
