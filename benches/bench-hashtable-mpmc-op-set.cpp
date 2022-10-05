/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <cstdint>
#include <numa.h>

#include <benchmark/benchmark.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "config.h"
#include "thread.h"
#include "memory_fences.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "spinlock.h"
#include "log/log.h"
#include "memory_fences.h"
#include "utils_cpu.h"
#include "fiber.h"
#include "fiber_scheduler.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"

#include "data_structures/hashtable/mcmp/hashtable_op_set.h"
#include "data_structures/hashtable/mcmp/hashtable_op_get.h"

#include "../tests/unit_tests/support.h"

#include "benchmark-program.hpp"
#include "benchmark-support.hpp"

// Set the generator to use
#define KEYSET_GENERATOR_METHOD     TEST_SUPPORT_RANDOM_KEYS_GEN_FUNC_RANDOM_STR_MAX_LENGTH

// It is possible to control the amount of threads used for the test tuning the two defines below
#define TEST_THREADS_RANGE_BEGIN (1)
#define TEST_THREADS_RANGE_END (utils_cpu_count())

// These two are kept static and external because
// - Google Benchmark invokes the setup multiple times, once per thread, but it doesn't have an entry point invoked
//   only once to setup non-shared elements for the test
// - the order of the threads started by Google Benchmark is undefined and therefore the code relies on the first thread
//   to setup the keyset and the db and then on having these two static pointers set to NULL to understand when they
//   have been configured by the thread 0. The other threads will wait for these allocations to be made.
volatile static hashtable_t *static_hashtable = nullptr;
volatile static test_support_keyset_slot_t *static_keyset_slots = nullptr;
volatile static bool static_storage_db_populated = false;

class HashtableOpSetInsertFixture : public benchmark::Fixture {
private:
    hashtable_t *_hashtable = nullptr;
    test_support_keyset_slot_t *_keyset_slots = nullptr;
    uint64_t _requested_keyset_size = 0;

public:
    hashtable_t *GetHashtable() {
        return this->_hashtable;
    }

    test_support_keyset_slot_t *GetKeysetSlots() {
        return this->_keyset_slots;
    }

    [[nodiscard]] uint64_t GetRequestedKeysetSize() const {
        return this->_requested_keyset_size;
    }

    void SetUp(const ::benchmark::State& state) override {
        char error_message[150] = {0};

        test_support_set_thread_affinity(state.thread_index());

        // Calculate the requested keyset size
        double requested_load_factor = (double) state.range(1) / 100.0f;
        this->_requested_keyset_size = (uint64_t) (((double) state.range(0)) * requested_load_factor);

        if (state.thread_index() == 0) {
            if (BenchmarkSupport::CheckIfTooManyThreadsPerCore(
                    state.threads(),
                    BENCHES_MAX_THREADS_PER_CORE)) {
                sprintf(error_message, "Too many threads per core, max allowed <%d>", BENCHES_MAX_THREADS_PER_CORE);
                ((::benchmark::State &) state).SkipWithError(error_message);

                return;
            }

            // Initialize the key set
            static_keyset_slots = test_support_init_keyset_slots(
                    this->_requested_keyset_size,
                    KEYSET_GENERATOR_METHOD,
                    544498304);

            // Setup the hashtable
            static_hashtable = test_support_init_hashtable(state.range(0));

            if (!static_hashtable) {
                sprintf(
                        error_message,
                        "Failed to allocate the hashtable, unable to continue");
                ((::benchmark::State &) state).SkipWithError(error_message);
                return;
            }

            MEMORY_FENCE_STORE();
        }

        if (state.thread_index() != 0) {
            while (!static_hashtable) {
                MEMORY_FENCE_LOAD();
                sched_yield();
            }

            while (!static_keyset_slots) {
                MEMORY_FENCE_LOAD();
                sched_yield();
            }
        }

        this->_hashtable = (hashtable_t *)static_hashtable;
        this->_keyset_slots = (test_support_keyset_slot_t *)static_keyset_slots;
    }

    void TearDown(const ::benchmark::State& state) override {
        if (state.thread_index() != 0) {
            return;
        }

        if (this->_hashtable != nullptr) {
            BenchmarkSupport::CollectHashtableStatsAndUpdateState(
                    (benchmark::State&)state, this->_hashtable);

            // Free the storage
            hashtable_mcmp_free(this->_hashtable);
        }

        if (this->_keyset_slots != nullptr) {
            // Free the keys
            test_support_free_keyset_slots(this->_keyset_slots);
        }

        this->_hashtable = nullptr;
        this->_keyset_slots = nullptr;
        this->_requested_keyset_size = 0;

        static_hashtable = nullptr;
        static_keyset_slots = nullptr;
    }
};

BENCHMARK_DEFINE_F(HashtableOpSetInsertFixture, hashtable_op_set_insert)(benchmark::State& state) {
    bool result;
    hashtable_t *hashtable;
    uint64_t requested_keyset_size;
    test_support_keyset_slot_t *keyset_slots;
    char error_message[150] = {0};

    test_support_set_thread_affinity(state.thread_index());

    // Fetch the information from the fixtures needed for the test
    hashtable = this->GetHashtable();
    keyset_slots = this->GetKeysetSlots();
    requested_keyset_size = this->GetRequestedKeysetSize();

    for (auto _ : state) {
        for(
                uint64_t key_index = state.thread_index();
                key_index < requested_keyset_size;
                key_index += state.threads()) {
            benchmark::DoNotOptimize((result = hashtable_mcmp_op_set(
                    hashtable,
                    keyset_slots[key_index].key,
                    keyset_slots[key_index].key_length,
                    key_index,
                    nullptr)));

            if (!result) {
                sprintf(
                        error_message,
                        "Unable to set the key <%s> with index <%ld> for the thread <%d>",
                        keyset_slots[key_index].key,
                        key_index,
                        state.thread_index());
                state.SkipWithError(error_message);
                break;
            }
        }
    }
}

class HashtableOpSetUpdateFixture : public benchmark::Fixture {
private:
    hashtable_t *_hashtable = nullptr;
    test_support_keyset_slot_t *_keyset_slots = nullptr;
    uint64_t _requested_keyset_size = 0;

public:
    hashtable_t *GetHashtable() {
        return this->_hashtable;
    }

    test_support_keyset_slot_t *GetKeysetSlots() {
        return this->_keyset_slots;
    }

    [[nodiscard]] uint64_t GetRequestedKeysetSize() const {
        return this->_requested_keyset_size;
    }

    void SetUp(const ::benchmark::State& state) override {
        char error_message[150] = {0};

        test_support_set_thread_affinity(state.thread_index());

        // Calculate the requested keyset size
        double requested_load_factor = (double) state.range(1) / 100.0f;
        this->_requested_keyset_size = (uint64_t) (((double) state.range(0)) * requested_load_factor);

        if (state.thread_index() == 0) {
            if (BenchmarkSupport::CheckIfTooManyThreadsPerCore(
                    state.threads(),
                    BENCHES_MAX_THREADS_PER_CORE)) {
                sprintf(error_message, "Too many threads per core, max allowed <%d>", BENCHES_MAX_THREADS_PER_CORE);
                ((::benchmark::State &) state).SkipWithError(error_message);

                return;
            }

            // Initialize the key set
            static_keyset_slots = test_support_init_keyset_slots(
                    this->_requested_keyset_size,
                    KEYSET_GENERATOR_METHOD,
                    544498304);

            // Setup the hashtable
            static_hashtable = test_support_init_hashtable(state.range(0));

            if (!static_hashtable) {
                sprintf(
                        error_message,
                        "Failed to allocate the hashtable, unable to continue");
                ((::benchmark::State &) state).SkipWithError(error_message);
                return;
            }

            MEMORY_FENCE_STORE();
        }

        if (state.thread_index() != 0) {
            while (!static_hashtable) {
                MEMORY_FENCE_LOAD();
                sched_yield();
            }

            while (!static_keyset_slots) {
                MEMORY_FENCE_LOAD();
                sched_yield();
            }
        }

        for(
                uint64_t key_index = state.thread_index();
                key_index < this->_requested_keyset_size;
                key_index += state.threads()) {
            bool result = hashtable_mcmp_op_set(
                    (hashtable_t*)static_hashtable,
                    static_keyset_slots[key_index].key,
                    static_keyset_slots[key_index].key_length,
                    key_index,
                    nullptr);

            if (!result) {
                sprintf(
                        error_message,
                        "Unable to set the key <%s (%d)> with index <%ld> for the thread <%d>",
                        static_keyset_slots[key_index].key,
                        static_keyset_slots[key_index].key_length,
                        key_index,
                        state.thread_index());

                ((::benchmark::State &) state).SkipWithError(error_message);
                break;
            }
        }

        if (state.thread_index() == 0) {
#if HASHTABLE_FLAG_ALLOW_KEY_INLINE == 1
            // Free up the memory allocated for the first keyset slots generated
            test_support_free_keyset_slots((test_support_keyset_slot_t *)static_keyset_slots);

            // Re-initialize the keyset, the random generator in use will re-generate exactly the same set as we are
            // using the same random seed (although the generator will not generate them in the same order because
            // threads are used but that's doesn't matter)
            static_keyset_slots = test_support_init_keyset_slots(
                    this->_requested_keyset_size,
                    KEYSET_GENERATOR_METHOD,
                    544498304);
#endif

            static_storage_db_populated = true;
            MEMORY_FENCE_STORE();
        }

        if (state.thread_index() != 0) {
            while (!static_storage_db_populated) {
                MEMORY_FENCE_LOAD();
                sched_yield();
            }
        }

        this->_hashtable = (hashtable_t *)static_hashtable;
        this->_keyset_slots = (test_support_keyset_slot_t *)static_keyset_slots;
    }

    void TearDown(const ::benchmark::State& state) override {
        if (state.thread_index() != 0) {
            return;
        }

        if (this->_hashtable != nullptr) {
            BenchmarkSupport::CollectHashtableStatsAndUpdateState(
                    (benchmark::State&)state, this->_hashtable);

            // Free the storage
            hashtable_mcmp_free(this->_hashtable);
        }

        if (this->_keyset_slots != nullptr) {
            // Free the keys
            test_support_free_keyset_slots(this->_keyset_slots);
        }

        this->_hashtable = nullptr;
        this->_keyset_slots = nullptr;
        this->_requested_keyset_size = 0;

        static_hashtable = nullptr;
        static_keyset_slots = nullptr;
        static_storage_db_populated = false;
    }
};

BENCHMARK_DEFINE_F(HashtableOpSetUpdateFixture, hashtable_op_set_update)(benchmark::State& state) {
    bool result;
    hashtable_t *hashtable;
    uint64_t requested_keyset_size;
    test_support_keyset_slot_t *keyset_slots;
    char error_message[150] = {0};

    test_support_set_thread_affinity(state.thread_index());

    // Fetch the information from the fixtures needed for the test
    hashtable = this->GetHashtable();
    keyset_slots = this->GetKeysetSlots();
    requested_keyset_size = this->GetRequestedKeysetSize();

    for (auto _ : state) {
        for(
                uint64_t key_index = state.thread_index();
                key_index < requested_keyset_size;
                key_index += state.threads()) {
            benchmark::DoNotOptimize((result = hashtable_mcmp_op_set(
                    hashtable,
                    keyset_slots[key_index].key,
                    keyset_slots[key_index].key_length,
                    key_index,
                    nullptr)));

            if (!result) {
                sprintf(
                        error_message,
                        "Unable to set the key <%s> with index <%ld> for the thread <%d>",
                        keyset_slots[key_index].key,
                        key_index,
                        state.thread_index());
                state.SkipWithError(error_message);
                break;
            }
        }
    }
}

static void BenchArguments(benchmark::internal::Benchmark* b) {
    b
            ->ArgsProduct({
                                  { 0x0000FFFFu, 0x000FFFFFu, 0x001FFFFFu, 0x007FFFFFu, 0x00FFFFFFu, 0x01FFFFFFu, 0x07FFFFFFu,
                                          0x0FFFFFFFu, 0x1FFFFFFFu, 0x3FFFFFFFu, 0x7FFFFFFFu },
                                  { 50, 75 },
                          })
            ->ThreadRange(TEST_THREADS_RANGE_BEGIN, TEST_THREADS_RANGE_END)
            ->Iterations(1)
            ->Repetitions(25)
            ->DisplayAggregatesOnly(false);
}


BENCHMARK_REGISTER_F(HashtableOpSetInsertFixture, hashtable_op_set_insert)
        ->Apply(BenchArguments);

BENCHMARK_REGISTER_F(HashtableOpSetUpdateFixture, hashtable_op_set_update)
        ->Apply(BenchArguments);
