/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "config.h"
#include "fiber/fiber.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"

#include "transaction.h"
#include "transaction_spinlock.h"

#define TAG "transaction_spinlock"

thread_local uint32_t transaction_manager_worker_index = 0;
thread_local uint16_t transaction_manager_transaction_index = 0;
pthread_once_t transaction_manager_init_once_control = PTHREAD_ONCE_INIT;

void transaction_set_worker_index(
        uint32_t worker_index) {
    transaction_manager_worker_index = worker_index;
}

void transaction_manager_init() {
    transaction_set_worker_index(worker_context_get()->worker_index);
    assert(transaction_manager_worker_index <= UINT16_MAX);
}

bool transaction_expand_locks_list(
        transaction_t *transaction) {
    size_t current_mem_size = sizeof(transaction_spinlock_lock_volatile_t*) * transaction->locks.size;

    transaction->locks.size = transaction->locks.size * 2;
    transaction->locks.list = ffma_mem_realloc(
            transaction->locks.list,
            current_mem_size,
            sizeof(transaction_spinlock_lock_volatile_t*) * transaction->locks.size,
            false);

    return transaction->locks.list != NULL;
}

uint16_t transaction_peek_current_thread_index() {
    return transaction_manager_transaction_index;
}

bool transaction_acquire(
        transaction_t *transaction) {
    pthread_once(&transaction_manager_init_once_control, transaction_manager_init);

    transaction_manager_transaction_index++;

    if (unlikely(transaction_manager_worker_index == 0 &&
        transaction_manager_transaction_index == TRANSACTION_ID_NOT_ACQUIRED)) {
        transaction_manager_transaction_index++;
    }

    transaction->transaction_id.worker_index = transaction_manager_worker_index;
    transaction->transaction_id.transaction_index = transaction_manager_transaction_index;

    transaction->locks.count = 0;
    transaction->locks.size = 8;
    transaction->locks.list = ffma_mem_alloc(
            sizeof(transaction_spinlock_lock_volatile_t*) * transaction->locks.size);

    if (transaction->locks.list == NULL) {
        return false;
    }

    return true;
}

void transaction_release(
        transaction_t *transaction) {
    assert(transaction->transaction_id.id != TRANSACTION_ID_NOT_ACQUIRED);

    for(uint32_t index = 0; index < transaction->locks.count; index++) {
#if DEBUG == 1
        transaction_spinlock_unlock_internal(transaction->locks.list[index], transaction);
#else
        transaction_spinlock_unlock_internal(transaction->locks.list[index]);
#endif
    }

    ffma_mem_free(transaction->locks.list);

    transaction->locks.list = NULL;
    transaction->transaction_id.id = TRANSACTION_ID_NOT_ACQUIRED;
}
