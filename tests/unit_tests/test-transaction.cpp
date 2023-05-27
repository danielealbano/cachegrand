/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>
#include <sys/syscall.h>

#include "misc.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "utils_cpu.h"
#include "thread.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "fiber/fiber.h"
#include "fiber/fiber_scheduler.h"
#include "clock.h"
#include "config.h"

#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"

TEST_CASE("transaction.c", "[transaction]") {
    worker_context_t worker_context = { 0 };
    worker_context.worker_index = UINT16_MAX;
    worker_context_set(&worker_context);
    transaction_set_worker_index(worker_context.worker_index);

    SECTION("transaction_expand_locks_list") {
        transaction_t transaction = { };
        transaction.locks.count = 0;
        transaction.locks.size = 8;

        transaction_spinlock_lock_volatile_t** initial_list = (transaction_spinlock_lock_volatile_t**)ffma_mem_alloc(
                sizeof(void*) * transaction.locks.size);
        transaction.locks.list = initial_list;

        SECTION("Expand once") {
            transaction_expand_locks_list(&transaction);

            REQUIRE(transaction.locks.size == 16);
            REQUIRE(transaction.locks.count == 0);
            REQUIRE(transaction.locks.list != initial_list);
        }

        SECTION("Expand twice") {
            transaction_expand_locks_list(&transaction);

            transaction_spinlock_lock_volatile_t** interim_list = transaction.locks.list;
            transaction_expand_locks_list(&transaction);

            REQUIRE(transaction.locks.size == 32);
            REQUIRE(transaction.locks.count == 0);
            REQUIRE(transaction.locks.list != interim_list);
        }

        ffma_mem_free(transaction.locks.list);
    }

    SECTION("transaction_needs_expand_locks_list") {
        SECTION("Needs expand") {
            transaction_t transaction = { };
            transaction.locks.count = 8;
            transaction.locks.size = 8;

            REQUIRE(transaction_needs_expand_locks_list(&transaction));
        }

        SECTION("Doesn't needs expand") {
            transaction_t transaction = { };
            transaction.locks.count = 7;
            transaction.locks.size = 8;

            REQUIRE(!transaction_needs_expand_locks_list(&transaction));
        }
    }

    SECTION("transaction_locks_list_add") {
        uint32_t locks_size = FFMA_OBJECT_SIZE_MIN / sizeof(transaction_spinlock_lock_volatile_t*);

        transaction_t transaction = { };
        transaction.locks.count = 0;
        transaction.locks.size = locks_size;
        transaction.locks.list = (transaction_spinlock_lock_volatile_t**)ffma_mem_alloc(
                sizeof(void*) * transaction.locks.size);

        SECTION("Add one lock") {
            transaction_spinlock_lock_volatile_t lock = { 0 };

            REQUIRE(transaction_locks_list_add(&transaction, &lock));
            REQUIRE(transaction.locks.count == 1);
            REQUIRE(transaction.locks.size == locks_size);
            REQUIRE(transaction.locks.list[0] == &lock);
        }

        SECTION("Add two lock") {
            transaction_spinlock_lock_volatile_t lock[2] = { 0 };
            REQUIRE(transaction_locks_list_add(&transaction, &lock[0]));
            REQUIRE(transaction.locks.count == 1);
            REQUIRE(transaction.locks.size == locks_size);
            REQUIRE(transaction.locks.list[0] == &lock[0]);

            REQUIRE(transaction_locks_list_add(&transaction, &lock[1]));
            REQUIRE(transaction.locks.count == 2);
            REQUIRE(transaction.locks.size == locks_size);
            REQUIRE(transaction.locks.list[1] == &lock[1]);
        }

        SECTION("Trigger an expansion expansion") {
            transaction_spinlock_lock_volatile_t lock[3] = { 0 };

            for(int index = 0; index < ARRAY_SIZE(lock); index++) {
                REQUIRE(transaction_locks_list_add(&transaction, &lock[index]));
            }

            REQUIRE(transaction.locks.count == ARRAY_SIZE(lock));
            REQUIRE(transaction.locks.size == locks_size * 2);
            REQUIRE(transaction.locks.list[ARRAY_SIZE(lock) - 1] == &lock[ARRAY_SIZE(lock) - 1]);
        }

        SECTION("Trigger multiple expansion") {
            transaction_spinlock_lock_volatile_t lock[10] = { 0 };

            for(int index = 0; index < ARRAY_SIZE(lock); index++) {
                REQUIRE(transaction_locks_list_add(&transaction, &lock[index]));
            }

            REQUIRE(transaction.locks.count == ARRAY_SIZE(lock));
            REQUIRE(transaction.locks.size == 16);
            REQUIRE(transaction.locks.list[ARRAY_SIZE(lock) - 1] == &lock[ARRAY_SIZE(lock) - 1]);
        }

        ffma_mem_free(transaction.locks.list);
    }

    SECTION("transaction_acquire") {
        SECTION("Acquire one") {
            transaction_t transaction = { 0 };

            uint16_t current_transaction_index = transaction_peek_current_thread_index();

            REQUIRE(transaction_acquire(&transaction));

            REQUIRE(transaction.transaction_id.worker_index == UINT16_MAX);
            REQUIRE(transaction.transaction_id.transaction_index == current_transaction_index + 1);

            REQUIRE(transaction.locks.count == 0);
            REQUIRE(transaction.locks.size == FFMA_OBJECT_SIZE_MIN / sizeof(transaction_spinlock_lock_volatile_t*));
            REQUIRE(transaction.locks.list != nullptr);

            ffma_mem_free(transaction.locks.list);
        }

        SECTION("Acquire two") {
            transaction_t transaction1 = { 0 };
            transaction_t transaction2 = { 0 };

            uint16_t current_transaction_index = transaction_peek_current_thread_index();

            REQUIRE(transaction_acquire(&transaction1));
            REQUIRE(transaction_acquire(&transaction2));

            REQUIRE(transaction2.transaction_id.worker_index == UINT16_MAX);
            REQUIRE(transaction2.transaction_id.transaction_index == current_transaction_index + 2);

            ffma_mem_free(transaction1.locks.list);
            ffma_mem_free(transaction2.locks.list);
        }
    }

    SECTION("transaction_release") {
        transaction_t transaction = { 0 };
        REQUIRE(transaction_acquire(&transaction));

        SECTION("Empty transaction") {
            transaction_release(&transaction);

            REQUIRE(transaction.locks.list == nullptr);
            REQUIRE(transaction.transaction_id.id == TRANSACTION_ID_NOT_ACQUIRED);
        }

        SECTION("With one lock") {
            transaction_spinlock_lock_volatile_t lock = { transaction.transaction_id.id };

            REQUIRE(transaction_locks_list_add(&transaction, &lock));

            transaction_release(&transaction);

            REQUIRE(lock.transaction_id == TRANSACTION_SPINLOCK_UNLOCKED);
            REQUIRE(transaction.locks.list == nullptr);
            REQUIRE(transaction.transaction_id.id == TRANSACTION_ID_NOT_ACQUIRED);
        }

        SECTION("With two locks") {
            transaction_spinlock_lock_volatile_t lock1 = { transaction.transaction_id.id };
            transaction_spinlock_lock_volatile_t lock2 = { transaction.transaction_id.id };

            REQUIRE(transaction_locks_list_add(&transaction, &lock1));
            REQUIRE(transaction_locks_list_add(&transaction, &lock2));

            transaction_release(&transaction);

            REQUIRE(lock1.transaction_id == TRANSACTION_SPINLOCK_UNLOCKED);
            REQUIRE(lock2.transaction_id == TRANSACTION_SPINLOCK_UNLOCKED);
            REQUIRE(transaction.locks.list == nullptr);
            REQUIRE(transaction.transaction_id.id == TRANSACTION_ID_NOT_ACQUIRED);
        }

        SECTION("Expanded") {
            transaction_spinlock_lock_volatile_t lock[10] = { 0 };

            for(int index = 0; index < ARRAY_SIZE(lock); index++) {
                lock[index].transaction_id = transaction.transaction_id.id;
                REQUIRE(transaction_locks_list_add(&transaction, &lock[index]));
            }

            transaction_release(&transaction);

            for(int index = 0; index < ARRAY_SIZE(lock); index++) {
                REQUIRE(lock[index].transaction_id == TRANSACTION_SPINLOCK_UNLOCKED);
            }
            REQUIRE(transaction.locks.list == nullptr);
            REQUIRE(transaction.transaction_id.id == TRANSACTION_ID_NOT_ACQUIRED);
        }
    }
}
