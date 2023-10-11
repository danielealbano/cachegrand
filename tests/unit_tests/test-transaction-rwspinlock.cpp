/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>
#include <unistd.h>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <atomic>
using namespace std;

#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"

#include "misc.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "utils_cpu.h"
#include "thread.h"
#include "xalloc.h"
#include "spinlock.h"
#include "transaction.h"
#include "fiber/fiber.h"
#include "fiber/fiber_scheduler.h"
#include "clock.h"
#include "config.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"
#include "transaction.h"

std::atomic<uint32_t> worker_index_counter(1);

// Returns 1 if it can do the initial lock, 2 instead if it's able to reach the point in which has
// to wait for the spinlock to become free
void* test_transaction_rwspinlock_lock_retry_try_lock_thread_func(void* rawdata) {
    worker_context_t worker_context = { 0 };
    worker_context.worker_index = worker_index_counter.fetch_add(1);
    worker_context_set(&worker_context);
    transaction_set_worker_index(worker_context.worker_index);

    transaction_t transaction = { .transaction_id = { }, .locks = { .count = 0 , .size = 0, .list = nullptr, } };
    transaction_id_volatile_t *transaction_id = &transaction.transaction_id;
    transaction_id->worker_index = 0; transaction_id->transaction_index = 0;
    auto* lock = (transaction_rwspinlock_t*)rawdata;

    transaction_acquire(&transaction);
    if (transaction_rwspinlock_try_write_lock_internal(lock, &transaction, 0)) {
        return (void*)1;
    }

    REQUIRE(transaction_rwspinlock_write_lock_internal(lock, &transaction, 0, "", 0));

    if (transaction_rwspinlock_is_write_locked(lock) == 1) {
        transaction_rwspinlock_unlock_internal(
                lock
#if DEBUG == 1
                ,&transaction
#endif
                );
        return (void*)2;
    } else {
        return (void*)1;
    }
}

void *test_transaction_rwspinlock_lock_wait_for_reader_thread_func(void* rawdata) {
    worker_context_t worker_context = { 0 };
    worker_context.worker_index = worker_index_counter.fetch_add(1);
    worker_context_set(&worker_context);
    transaction_set_worker_index(worker_context.worker_index);

    transaction_t transaction = { .transaction_id = { }, .locks = { .count = 0 , .size = 0, .list = nullptr, } };
    transaction_id_volatile_t *transaction_id = &transaction.transaction_id;
    transaction_id->worker_index = 0; transaction_id->transaction_index = 0;
    auto* lock = (transaction_rwspinlock_t*)rawdata;

    transaction_acquire(&transaction);
    if (transaction_rwspinlock_try_write_lock_internal(lock, &transaction, 0)) {
        return (void*)1;
    }

    REQUIRE(transaction_rwspinlock_write_lock_internal(lock, &transaction, 0, "", 0));

    if (transaction_rwspinlock_is_write_locked(lock) == 1) {
        transaction_rwspinlock_unlock_internal(
                lock
#if DEBUG == 1
                ,&transaction
#endif
        );
        return (void*)2;
    } else {
        return (void*)1;
    }
}

void test_transaction_rwspinlock_thread_wait_on_flag(
        const volatile bool *flag,
        bool expecting_value) {
    do {
        MEMORY_FENCE_LOAD();
    } while (*flag != expecting_value);
}

// Increments a number of times using the spinlock for each increment
struct test_transaction_rwspinlock_lock_counter_thread_func_data {
    bool* start_flag;
    uint32_t thread_num;
    pthread_t thread_id;
    transaction_rwspinlock_volatile_t* lock;
    uint64_t increments;
    uint64_t* counter;
};
void* test_transaction_rwspinlock_lock_counter_thread_func(void* rawdata) {
    auto* data = (struct test_transaction_rwspinlock_lock_counter_thread_func_data*)rawdata;

    worker_context_t worker_context = { 0 };
    worker_context.worker_index = data->thread_num;
    worker_context_set(&worker_context);

    thread_current_set_affinity(data->thread_num);

    test_transaction_rwspinlock_thread_wait_on_flag(data->start_flag, true);

    for(uint64_t i = 0; i < data->increments; i++) {
        transaction_t transaction = { .transaction_id = { }, .locks = { .count = 0 , .size = 0, .list = nullptr, } };
        transaction_id_volatile_t *transaction_id = &transaction.transaction_id;
        transaction_id->worker_index = 0; transaction_id->transaction_index = 0;

        transaction_acquire(&transaction);

        REQUIRE(transaction_rwspinlock_write_lock_internal(data->lock, &transaction, 0, "", 0));
        (*data->counter)++;

        transaction_rwspinlock_unlock_internal(
                data->lock
#if DEBUG == 1
                ,&transaction
#endif
        );
    }

    return nullptr;
}

TEST_CASE("transaction.c (transaction_rwspinlock)", "[transaction][transaction_rwspinlock]") {
    worker_context_t worker_context = { 0 };
    worker_context.worker_index = UINT16_MAX;
    worker_context_set(&worker_context);
    transaction_set_worker_index(worker_context.worker_index);

    SECTION("sizeof(transaction_rwspinlock_t) == 4") {
        REQUIRE(sizeof(transaction_rwspinlock_t) == 8);
    }

    SECTION("transaction_rwspinlock_init") {
        transaction_rwspinlock_t lock = {0};
        transaction_rwspinlock_init(&lock);

        REQUIRE(lock.internal_data.transaction_id == TRANSACTION_SPINLOCK_UNLOCKED);
    }

    SECTION("transaction_rwspinlock_is_write_locked") {
        transaction_rwspinlock_t lock = {0};
        transaction_rwspinlock_init(&lock);

        lock.internal_data.transaction_id = 1;

        REQUIRE(transaction_rwspinlock_is_write_locked(&lock));
    }

    SECTION("transaction_rwspinlock_try_write_lock_internal") {
        SECTION("lock") {
            transaction_t transaction = { .transaction_id = { }, .locks = { .count = 0 , .size = 0, .list = nullptr, } };
            transaction_id_volatile_t *transaction_id = &transaction.transaction_id;
            transaction_id->worker_index = 0; transaction_id->transaction_index = 0;
            transaction_rwspinlock_t lock = {0};
            transaction_rwspinlock_init(&lock);

            transaction_acquire(&transaction);
            REQUIRE(transaction_rwspinlock_try_write_lock_internal(&lock, &transaction, 0));
            REQUIRE(lock.internal_data.transaction_id != TRANSACTION_SPINLOCK_UNLOCKED);
            transaction_rwspinlock_unlock_internal(
                    &lock
#if DEBUG == 1
                    ,&transaction
#endif
            );
        }

        SECTION("lock - fail already locked") {
            transaction_rwspinlock_t lock = {0};
            transaction_rwspinlock_init(&lock);

            lock.internal_data.transaction_id = 1;

            REQUIRE(transaction_rwspinlock_is_write_locked(&lock));

            REQUIRE(lock.internal_data.transaction_id != TRANSACTION_SPINLOCK_UNLOCKED);
        }
    }

    SECTION("transaction_rwspinlock_unlock") {
        SECTION("unlock") {
            transaction_t transaction = { .transaction_id = { }, .locks = { .count = 0 , .size = 0, .list = nullptr, } };
            transaction_id_volatile_t *transaction_id = &transaction.transaction_id;
            transaction_id->worker_index = 0; transaction_id->transaction_index = 1;
            transaction_rwspinlock_t lock = {0};
            transaction_rwspinlock_init(&lock);

            lock.internal_data.transaction_id = transaction_id->id;

#if DEBUG == 1
            transaction_rwspinlock_unlock_internal(&lock, &transaction);
#else
            transaction_rwspinlock_unlock_internal(&lock);
#endif
            REQUIRE(lock.internal_data.transaction_id == TRANSACTION_SPINLOCK_UNLOCKED);
        }
    }

    SECTION("transaction_rwspinlock_lock") {
        SECTION("lock") {
            int res, pthread_return_val, *pthread_return = nullptr;
            transaction_t transaction = { .transaction_id = { }, .locks = { .count = 0 , .size = 0, .list = nullptr, } };
            transaction_id_volatile_t *transaction_id = &transaction.transaction_id;
            transaction_id->worker_index = 0; transaction_id->transaction_index = 0;
            transaction_rwspinlock_t lock = {0};
            pthread_t pthread;
            pthread_attr_t attr;

            transaction_rwspinlock_init(&lock);

            res = pthread_attr_init(&attr);
            if (res != 0) {
                perror("pthread_attr_init");
            }

            transaction_acquire(&transaction);

            // Lock
            REQUIRE(transaction_rwspinlock_try_write_lock_internal(&lock, &transaction, 0));
            REQUIRE(lock.internal_data.transaction_id == transaction_id->id);

            // Create the thread that wait for unlock
            res = pthread_create(&pthread, &attr, &test_transaction_rwspinlock_lock_retry_try_lock_thread_func, (void*)&lock);
            if (res != 0) {
                perror("pthread_create");
            }

            // Wait
            usleep(100000);

            // Unlock
            transaction_rwspinlock_unlock_internal(
                    &lock
#if DEBUG == 1
                    ,&transaction
#endif
            );

            // Wait
            usleep(1000000);

            // TODO: would be better to come up with a better mechanism to check if the thread has actually finished or
            //       not but the above sleep seems reasonable, the thread doesn't finish within 1 second
            int pthread_tryjoin_np_res = pthread_tryjoin_np(pthread, (void**)&pthread_return);
            pthread_return_val = (int)(uint64_t)pthread_return;

            REQUIRE(pthread_tryjoin_np_res == 0);
            REQUIRE(pthread_return_val == 2);

            // The difference between this test and the plain spinlock one is that the thread does a transaction release
            // therefore the lock gets unlocked
            REQUIRE(lock.internal_data.transaction_id == TRANSACTION_SPINLOCK_UNLOCKED);
        }

        SECTION("lock - wait for reader") {
            int res, pthread_return_val, *pthread_return = nullptr;
            transaction_t transaction = { .transaction_id = { }, .locks = { .count = 0 , .size = 0, .list = nullptr, } };
            transaction_id_volatile_t *transaction_id = &transaction.transaction_id;
            transaction_id->worker_index = 0; transaction_id->transaction_index = 0;
            transaction_rwspinlock_t lock = {0};
            pthread_t pthread;
            pthread_attr_t attr;

            transaction_rwspinlock_init(&lock);

            res = pthread_attr_init(&attr);
            if (res != 0) {
                perror("pthread_attr_init");
            }

            transaction_acquire(&transaction);

            // Set the reader counter to 1
            lock.internal_data.readers_count = 1;

            // Create the thread that wait for unlock
            res = pthread_create(&pthread, &attr, &test_transaction_rwspinlock_lock_wait_for_reader_thread_func, (void*)&lock);
            if (res != 0) {
                perror("pthread_create");
            }

            // Wait
            usleep(100000);

            // Reset the reader counter
            lock.internal_data.readers_count = 0;
            MEMORY_FENCE_STORE();

            // Wait
            usleep(1000000);

            // TODO: would be better to come up with a better mechanism to check if the thread has actually finished or
            //       not but the above sleep seems reasonable, the thread doesn't finish within 1 second
            int pthread_tryjoin_np_res = pthread_tryjoin_np(pthread, (void**)&pthread_return);
            pthread_return_val = (int)(uint64_t)pthread_return;

            REQUIRE(pthread_tryjoin_np_res == 0);
            REQUIRE(pthread_return_val == 2);

            // The difference between this test and the plain spinlock one is that the thread does a transaction release
            // therefore the lock gets unlocked
            REQUIRE(lock.internal_data.transaction_id == TRANSACTION_SPINLOCK_UNLOCKED);
        }

        SECTION("fail - already locked") {
            transaction_t transaction = { .transaction_id = { }, .locks = { .count = 0 , .size = 0, .list = nullptr, } };
            transaction_id_volatile_t *transaction_id = &transaction.transaction_id;
            transaction_id->worker_index = 0; transaction_id->transaction_index = 0;
            transaction_rwspinlock_t lock = {0};
            transaction_rwspinlock_init(&lock);

            transaction_acquire(&transaction);

            REQUIRE(transaction_rwspinlock_write_lock_internal(&lock, &transaction, 0, "", 0));
            REQUIRE(lock.internal_data.transaction_id == transaction_id->id);
            REQUIRE(!transaction_rwspinlock_try_write_lock_internal(&lock, &transaction, 0));

            transaction_rwspinlock_unlock_internal(
                    &lock
#if DEBUG == 1
                    ,&transaction
#endif
            );
        }

        SECTION("fail - readers") {
            transaction_t transaction = { .transaction_id = { }, .locks = { .count = 0 , .size = 0, .list = nullptr, } };
            transaction_id_volatile_t *transaction_id = &transaction.transaction_id;
            transaction_id->worker_index = 0; transaction_id->transaction_index = 0;
            transaction_rwspinlock_t lock = {0};
            transaction_rwspinlock_init(&lock);

            transaction_acquire(&transaction);

            lock.internal_data.readers_count = 1;
            REQUIRE(!transaction_rwspinlock_try_write_lock_internal(&lock, &transaction, 0));
        }

        SECTION("test lock parallelism") {
            void* ret;
            bool start_flag;
            transaction_rwspinlock_volatile_t lock = { 0 };
            pthread_attr_t attr;
            uint64_t increments_per_thread_sum = 0, increments_per_thread;

            uint32_t cores_count = MIN(utils_cpu_count(), 8);

            // Magic numbers to run the threads in parallel for a few seconds after the threads creation.
#if DEBUG
            increments_per_thread = 1000;
#else
            increments_per_thread = 1000000;
#endif

            REQUIRE(pthread_attr_init(&attr) == 0);

            auto* threads_info = (struct test_transaction_rwspinlock_lock_counter_thread_func_data*)calloc(
                    cores_count,
                    sizeof(test_transaction_rwspinlock_lock_counter_thread_func_data));

            REQUIRE(threads_info != NULL);

            start_flag = false;
            transaction_rwspinlock_init(&lock);
            for(uint32_t core_num = 0; core_num < cores_count; core_num++) {
                threads_info[core_num].thread_num = core_num;
                threads_info[core_num].start_flag = &start_flag;
                threads_info[core_num].lock = &lock;
                threads_info[core_num].increments = increments_per_thread;
                threads_info[core_num].counter = &increments_per_thread_sum;

                REQUIRE(pthread_create(
                        &threads_info[core_num].thread_id,
                        &attr,
                        &test_transaction_rwspinlock_lock_counter_thread_func,
                        &threads_info[core_num]) == 0);
            }

            start_flag = true;
            MEMORY_FENCE_STORE();

            // Wait for all the threads to finish
            for(uint32_t core_num = 0; core_num < cores_count; core_num++) {
                REQUIRE(pthread_join(threads_info[core_num].thread_id, &ret) == 0);
            }

            free(threads_info);

            REQUIRE(increments_per_thread_sum == increments_per_thread * cores_count);
        }
    }
}
