#include <catch2/catch.hpp>

#include <atomic>
using namespace std;

#include <pthread.h>
#include <unistd.h>
#if DEBUG == 1
#include <sys/types.h>
#include <sys/syscall.h>
#endif

#include "exttypes.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "utils_cpu.h"
#include "thread.h"

// Returns 1 if it can do the initial lock, 2 instead if it's able to reach the point in which has
// to wait for the spinlock to become free
void* test_spinlock_lock__lock_retry_try_lock_thread_func(void* rawdata) {
    spinlock_lock_t* lock = (spinlock_lock_t*)rawdata;
    if (spinlock_lock(lock, false)) {
        return (void*)1;
    }

    spinlock_lock(lock, true);

    if (spinlock_is_locked(lock) == 1) {
        return (void*)2;
    } else {
        return (void*)1;
    }
}

// Increments a number a number of times using the spinlock for each increment
struct test_spinlock_lock__counter_thread_func_data {
    uint8_t* start_flag;
    uint32_t thread_num;
    pthread_t thread_id;
    spinlock_lock_t* lock;
    uint64_t increments;
    uint64_t* counter;
};
void* test_spinlock_lock__counter_thread_func(void* rawdata) {
    struct test_spinlock_lock__counter_thread_func_data* data =
            (struct test_spinlock_lock__counter_thread_func_data*)rawdata;

    thread_current_set_affinity(data->thread_num);

    do {
        HASHTABLE_MEMORY_FENCE_LOAD();
    } while (*data->start_flag == 0);

    for(uint64_t i = 0; i < data->increments; i++) {
        spinlock_lock(data->lock, true);
        (*data->counter)++;
        spinlock_unlock(data->lock);
    }

    return nullptr;
}

TEST_CASE("spinlock.c", "[spinlock]") {
    SECTION("sizeof(spinlock_lock_t) == 4") {
        REQUIRE(sizeof(spinlock_lock_t) == 4);
    }

    SECTION("spinlock_init") {
        spinlock_lock_t lock = {0};
        spinlock_init(&lock);

        REQUIRE(lock.lock == SPINLOCK_UNLOCKED);

#if DEBUG == 1
        REQUIRE(lock.magic == SPINLOCK_MAGIC);
#endif
    }

    SECTION("spinlock_is_locked") {
        spinlock_lock_t lock = {0};
        spinlock_init(&lock);

        lock.lock = SPINLOCK_LOCKED;

        REQUIRE(spinlock_is_locked(&lock));

#if DEBUG == 1
        REQUIRE(lock.magic == SPINLOCK_MAGIC);
#endif
    }

    SECTION("spinlock_try_lock") {
        SECTION("lock") {
            spinlock_lock_t lock = {0};
            spinlock_init(&lock);

            REQUIRE(spinlock_try_lock(&lock));
#if DEBUG == 1
            long thread_id = syscall(__NR_gettid);
            REQUIRE(lock.lock == (uint8_t)thread_id);
            REQUIRE(lock.magic == SPINLOCK_MAGIC);
#else
            REQUIRE(lock.lock == SPINLOCK_LOCKED);
#endif
        }

        SECTION("lock - fail already locked") {
            spinlock_lock_t lock = {0};
            spinlock_init(&lock);

            lock.lock = SPINLOCK_LOCKED;

            REQUIRE(spinlock_is_locked(&lock));
            REQUIRE(lock.lock == SPINLOCK_LOCKED);

#if DEBUG == 1
            REQUIRE(lock.magic == SPINLOCK_MAGIC);
#endif
        }
    }

    SECTION("spinlock_unlock") {
        SECTION("unlock") {
            spinlock_lock_t lock = {0};
            spinlock_init(&lock);

            REQUIRE(spinlock_lock(&lock, false));
            spinlock_unlock(&lock);
            REQUIRE(lock.lock == SPINLOCK_UNLOCKED);
        }

        SECTION("unlock - already locked") {
            spinlock_lock_t lock = {0};
            spinlock_init(&lock);

            spinlock_unlock(&lock);
            REQUIRE(lock.lock == SPINLOCK_UNLOCKED);
        }
    }

    SECTION("spinlock_lock") {
        SECTION("lock") {
            spinlock_lock_t lock = {0};
            spinlock_init(&lock);

            REQUIRE(spinlock_lock(&lock, false));

#if DEBUG == 1
            long thread_id = syscall(__NR_gettid);
            REQUIRE(lock.lock == (uint8_t)thread_id);
            REQUIRE(lock.magic == SPINLOCK_MAGIC);
#else
            REQUIRE(lock.lock == SPINLOCK_LOCKED);
#endif
        }

        SECTION("fail already locked") {
            spinlock_lock_t lock = {0};
            spinlock_init(&lock);

            REQUIRE(spinlock_lock(&lock, false));

            REQUIRE(!spinlock_lock(&lock, false));

#if DEBUG == 1
            long thread_id = syscall(__NR_gettid);
            REQUIRE(lock.lock == (uint8_t)thread_id);
            REQUIRE(lock.magic == SPINLOCK_MAGIC);
#else
            REQUIRE(lock.lock == SPINLOCK_LOCKED);
#endif
        }

        SECTION("lock with retry") {
            int res, pthread_return_val, *pthread_return = 0;
            spinlock_lock_t lock = {0};
            pthread_t pthread;
            pthread_attr_t attr;

            spinlock_init(&lock);

            res = pthread_attr_init(&attr);
            if (res != 0) {
                perror("pthread_attr_init");
            }

            // Lock
            REQUIRE(spinlock_lock(&lock, false));

#if DEBUG == 1
            long thread_id = syscall(__NR_gettid);
            REQUIRE(lock.lock == (uint8_t)thread_id);
            REQUIRE(lock.magic == SPINLOCK_MAGIC);
#else
            REQUIRE(lock.lock == SPINLOCK_LOCKED);
#endif

            // Create the thread that wait for unlock
            res = pthread_create(&pthread, &attr, &test_spinlock_lock__lock_retry_try_lock_thread_func, (void*)&lock);
            if (res != 0) {
                perror("pthread_create");
            }

            // Wait
            usleep(100000);

            // Unlock
            spinlock_unlock(&lock);

            // Wait
            usleep(1000000);

            // TODO: would be better to come up with a better mechanism to check if the thread has actually finished or
            //       not but the above sleep seems reasonable, the thread doesn't finish within 1 second the spinlock
            //       is buggy and stuck
            int pthread_tryjoin_np_res = pthread_tryjoin_np(pthread, (void**)&pthread_return);
            pthread_return_val = (uint64_t)pthread_return;

            REQUIRE(pthread_tryjoin_np_res == 0);
            REQUIRE(pthread_return_val == 2);
            REQUIRE(lock.lock != SPINLOCK_UNLOCKED);
        }

        SECTION("test lock parallelism") {
            void* ret;
            uint8_t start_flag;
            spinlock_lock_t lock = { 0 };
            pthread_attr_t attr;
            uint64_t increments_per_thread_sum = 0, increments_per_thread;

            uint32_t cores_count = utils_cpu_count();
            uint32_t threads_count = cores_count * 2;

            // Magic numbers to run enough thread in parallel for 1-2s after the thread creation.
            // The test can be quite time consuming when with an attached debugger.
            increments_per_thread = (uint64_t)(20000 /  ((float)threads_count / 48.0));

            REQUIRE(pthread_attr_init(&attr) == 0);

            struct test_spinlock_lock__counter_thread_func_data* threads_info =
                    (struct test_spinlock_lock__counter_thread_func_data*)calloc(
                            threads_count,
                            sizeof(test_spinlock_lock__counter_thread_func_data));

            REQUIRE(threads_info != NULL);

            start_flag = 0;
            spinlock_init(&lock);
            for(uint32_t thread_num = 0; thread_num < threads_count; thread_num++) {
                threads_info[thread_num].thread_num = thread_num;
                threads_info[thread_num].start_flag = &start_flag;
                threads_info[thread_num].lock = &lock;
                threads_info[thread_num].increments = increments_per_thread;
                threads_info[thread_num].counter = &increments_per_thread_sum;

                REQUIRE(pthread_create(
                        &threads_info[thread_num].thread_id,
                        &attr,
                        &test_spinlock_lock__counter_thread_func,
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
