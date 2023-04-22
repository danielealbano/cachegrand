/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <cstdint>
#include <cstring>
#include <numa.h>

#include <benchmark/benchmark.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "log/log.h"
#include "clock.h"
#include "memory_fences.h"
#include "config.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "utils_cpu.h"
#include "fiber/fiber.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"

#include "../tests/support.h"

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
volatile static storage_db *static_db = nullptr;
volatile static test_support_keyset_slot_t *static_keyset_slots = nullptr;

class StorageDbOpSetInsertFixture : public benchmark::Fixture {
private:
    storage_db *_db = nullptr;
    uint32_t _workers_count = 0;
    char _value_buffer[100] = { 0 };
    size_t _value_buffer_length = sizeof(_value_buffer);
    test_support_keyset_slot_t *_keyset_slots = nullptr;
    uint64_t _requested_keyset_size = 0;

public:
    storage_db *GetDb() {
        return this->_db;
    }

    [[nodiscard]] uint32_t GetWorkersCount() const {
        return this->_workers_count;
    }

    char *GetValueBuffer() {
        return this->_value_buffer;
    }

    [[nodiscard]] size_t GetValueBufferLength() const {
        return this->_value_buffer_length;
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

            char charset[] = {TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_REPEATED_LIST};
            // Generate the value (just some data from the charset)
            for (int i = 0; i < this->_value_buffer_length; i++) {
                this->_value_buffer[i] = charset[i % sizeof(charset)];
            }

            // Initialize the key set
            static_keyset_slots = test_support_init_keyset_slots(
                    this->_requested_keyset_size,
                    KEYSET_GENERATOR_METHOD,
                    544498304);

            // Setup the storage db
            storage_db_config_t *db_config = storage_db_config_new();
            db_config->max_keys = state.range(0);
            db_config->backend_type = STORAGE_DB_BACKEND_TYPE_MEMORY;
            static_db = storage_db_new(db_config, state.threads());
            if (!static_db) {
                storage_db_config_free(db_config);

                sprintf(
                        error_message,
                        "Failed to allocate the storage db, unable to continue");
                ((::benchmark::State &) state).SkipWithError(error_message);
                return;
            }

            MEMORY_FENCE_STORE();
        }

        if (state.thread_index() != 0) {
            while (!static_db) {
                MEMORY_FENCE_LOAD();
                sched_yield();
            }

            while (!static_keyset_slots) {
                MEMORY_FENCE_LOAD();
                sched_yield();
            }
        }

        this->_db = (storage_db*)static_db;

        this->_workers_count = state.threads();
        this->_keyset_slots = (test_support_keyset_slot_t *)static_keyset_slots;
    }

    void TearDown(const ::benchmark::State& state) override {
        if (state.thread_index() != 0) {
            return;
        }

        if (this->_db != nullptr) {
            BenchmarkSupport::CollectHashtableStatsAndUpdateState(
                    (benchmark::State&)state, this->_db->hashtable);

            // Free the storage
            storage_db_free(this->_db, this->_workers_count);
        }

        if (this->_keyset_slots != nullptr) {
            // Free the keys
            test_support_free_keyset_slots(this->_keyset_slots);
        }

        this->_db = nullptr;
        this->_workers_count = 0;
        this->_keyset_slots = nullptr;
        this->_requested_keyset_size = 0;

        static_db = nullptr;
        static_keyset_slots = nullptr;
    }
};

BENCHMARK_DEFINE_F(StorageDbOpSetInsertFixture, storage_db_op_set_insert)(benchmark::State& state) {
    bool result;
    uint64_t requested_keyset_size;
    test_support_keyset_slot_t *keyset_slots;
    worker_context_t *worker_context;
    char *value_buffer;
    size_t value_buffer_length;
    char error_message[150] = { 0 };

    test_support_set_thread_affinity(state.thread_index());

    // Set up the worker context, as it's required by the storage db, this has to be done here as the worker_context
    // is stored in a thread variable an the threads are managed internally by the benchmarking library and therefore
    // they can be recycled or re-created.
    if ((worker_context = worker_context_get()) == nullptr) {
        // This assigned memory will be lost but this is a benchmark and we don't care
        worker_context = (worker_context_t *)ffma_mem_alloc(sizeof(worker_context_t));
        worker_context_set(worker_context);
    }

    // Setup the worker as needed
    worker_context->worker_index = state.thread_index();
    worker_context->workers_count = this->GetWorkersCount();
    worker_context->db = this->GetDb();

    // Fetch the information from the fixtures needed for the test
    keyset_slots = this->GetKeysetSlots();
    value_buffer = this->GetValueBuffer();
    value_buffer_length = this->GetValueBufferLength();
    requested_keyset_size = this->GetRequestedKeysetSize();

    for (auto _ : state) {
        for(
                uint64_t key_index = state.thread_index();
                key_index < requested_keyset_size;
                key_index += state.threads()) {
            result = storage_db_set_small_value(
                    worker_context->db,
                    keyset_slots[key_index].key,
                    keyset_slots[key_index].key_length,
                    value_buffer,
                    value_buffer_length);

            if (!result) {
                sprintf(
                        error_message,
                        "Unable to set the key <%s (%d)> with index <%ld> for the thread <%d>",
                        keyset_slots[key_index].key,
                        keyset_slots[key_index].key_length,
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

BENCHMARK_REGISTER_F(StorageDbOpSetInsertFixture, storage_db_op_set_insert)
        ->Apply(BenchArguments);

// TODO: implement upsert benchmark

