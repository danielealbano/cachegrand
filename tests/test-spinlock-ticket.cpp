#include <catch2/catch.hpp>

#include <atomic>
using namespace std;

#include <assert.h>
#include <pthread.h>
#include <unistd.h>

#include "exttypes.h"
#include "memory_fences.h"
#include "spinlock_ticket.h"
#include "utils_cpu.h"
#include "log_debug.h"

void* test_spinlock_ticket_lock_lock_retry_try_lock_thread_func(void* rawdata) {
    spinlock_ticket_lock_volatile_t* lock = (spinlock_ticket_lock_volatile_t*)rawdata;

    spinlock_ticket_number_t ticket_number = spinlock_ticket_lock(lock);
    spinlock_ticket_unlock(lock);

    return NULL;
}

// Increments a number a number of times using the spinlock_ticket for each increment
struct test_spinlock_ticket_lock_counter_thread_func_data {
    uint8_t* start_flag;
    pthread_t thread_id;
    spinlock_ticket_lock_volatile_t* lock;
    uint64_t increments;
    uint64_t* counter;
};
void* test_spinlock_ticket_lock_counter_thread_func(void* rawdata) {
    struct test_spinlock_ticket_lock_counter_thread_func_data* data =
            (struct test_spinlock_ticket_lock_counter_thread_func_data*)rawdata;
    do {
        HASHTABLE_MEMORY_FENCE_LOAD();
    } while (*data->start_flag == 0);

    for(uint64_t i = 0; i < data->increments; i++) {
        spinlock_ticket_number_t ticket_number = spinlock_ticket_lock(data->lock);
        (*data->counter)++;
        assert(data->lock->serving == ticket_number);
        spinlock_ticket_unlock(data->lock);
    }

    return nullptr;
}

TEST_CASE("spinlock_ticket.c", "[spinlock_ticket]") {
    SECTION("sizeof(spinlock_ticket_lock_volatile_t) == 4") {
        REQUIRE(sizeof(spinlock_ticket_lock_volatile_t) == 4);
    }

    SECTION("spinlock_ticket_init") {
        spinlock_ticket_lock_volatile_t lock = {0};
        spinlock_ticket_init(&lock);

        REQUIRE(lock.available == 0);
        REQUIRE(lock.serving == 0);
    }

    SECTION("spinlock_ticket_unlock") {
        SECTION("unlock") {
            spinlock_ticket_lock_volatile_t lock = {0};
            spinlock_ticket_init(&lock);

            lock.available = 1;

            spinlock_ticket_unlock(&lock);

            REQUIRE(lock.serving == 1);
        }
    }

    SECTION("spinlock_ticket_lock") {
        SECTION("lock") {
            spinlock_ticket_lock_volatile_t lock = {0};
            spinlock_ticket_init(&lock);

            REQUIRE(spinlock_ticket_lock(&lock) == 0);

            REQUIRE(lock.available == 1);
            REQUIRE(lock.serving == 0);
        }

        SECTION("lock wait") {
            int res, pthread_return_val, *pthread_return = 0;
            spinlock_ticket_lock_volatile_t lock = {0};
            pthread_t pthread;
            pthread_attr_t attr;

            spinlock_ticket_init(&lock);

            res = pthread_attr_init(&attr);
            if (res != 0) {
                perror("pthread_attr_init");
            }

            // Lock
            REQUIRE(spinlock_ticket_lock(&lock) == 0);
            REQUIRE(lock.available == 1);
            REQUIRE(lock.serving == 0);

            // Create the thread that wait for unlock
            res = pthread_create(&pthread, &attr, &test_spinlock_ticket_lock_lock_retry_try_lock_thread_func, (void*)&lock);
            if (res != 0) {
                perror("pthread_create");
            }

            // Wait
            usleep(100000);

            // Unlock but first ensure that the new thread wasn't able to increase the serving (unlock)
            REQUIRE(lock.serving == 0);
            spinlock_ticket_unlock(&lock);

            // Wait
            usleep(1000000);

            // TODO: would be better to come up with a better mechanism to check if the thread has actually finished or
            //       not but the above sleep seems reasonable, the thread doesn't finish within 1 second the spinlock_ticket
            //       is buggy and stuck
            int pthread_tryjoin_np_res = pthread_tryjoin_np(pthread, (void**)&pthread_return);
            pthread_return_val = (uint64_t)pthread_return;

            REQUIRE(pthread_tryjoin_np_res == 0);
            REQUIRE(pthread_return_val == 0);
            REQUIRE(lock.available == 2);
            REQUIRE(lock.serving == 2);
        }

        SECTION("test lock parallelism") {
            void* ret;
            uint8_t start_flag;
            spinlock_ticket_lock_volatile_t lock = { 0 };
            pthread_attr_t attr;
            uint64_t increments_per_thread_sum = 0, increments_per_thread;

            uint32_t cores_count = utils_cpu_count();
            uint32_t threads_count = cores_count;

            // Magic numbers to run enough thread in parallel for 1-2s after the thread creation.
            // The test can be quite time consuming when with an attached debugger.
            increments_per_thread = (uint64_t)(50 /  ((float)threads_count / 24.0));

            REQUIRE(pthread_attr_init(&attr) == 0);

            struct test_spinlock_ticket_lock_counter_thread_func_data* threads_info =
                    (struct test_spinlock_ticket_lock_counter_thread_func_data*)calloc(
                            threads_count,
                            sizeof(test_spinlock_ticket_lock_counter_thread_func_data));

            REQUIRE(threads_info != NULL);

            start_flag = 0;
            spinlock_ticket_init(&lock);
            for(uint32_t thread_num = 0; thread_num < threads_count; thread_num++) {
                threads_info[thread_num].start_flag = &start_flag;
                threads_info[thread_num].lock = &lock;
                threads_info[thread_num].increments = increments_per_thread;
                threads_info[thread_num].counter = &increments_per_thread_sum;

                REQUIRE(pthread_create(
                        &threads_info[thread_num].thread_id,
                        &attr,
                        &test_spinlock_ticket_lock_counter_thread_func,
                        &threads_info[thread_num]) == 0);
            }

            start_flag = 1;
            HASHTABLE_MEMORY_FENCE_STORE();

            // Wait for all the threads to finish
            for(uint32_t thread_num = 0; thread_num < threads_count; thread_num++) {
                REQUIRE(pthread_join(threads_info[thread_num].thread_id, &ret) == 0);
            }

            free(threads_info);

            REQUIRE(increments_per_thread_sum == increments_per_thread * threads_count);
        }
    }
}
