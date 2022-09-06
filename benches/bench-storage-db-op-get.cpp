/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <cstdint>
#include <cstring>

#include <benchmark/benchmark.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "log/log.h"
#include "clock.h"
#include "memory_fences.h"
#include "config.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "memory_allocator/ffma.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "utils_cpu.h"
#include "fiber.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"

#include "../tests/support.h"
#include "benchmark-support.hpp"

#include "benchmark-program.hpp"

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
volatile static bool static_storage_db_populated = false;

class StorageDbOpGetFixture : public benchmark::Fixture {
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
            fprintf(stdout, "> Setup - started\n");
            fflush(stdout);

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

            fprintf(stdout, "> Setup - setting up storage_db\n");
            fflush(stdout);

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

            fprintf(stdout, "> Setup - initial keyset slots generation\n");
            fflush(stdout);

            // Initialize the key set
            static_keyset_slots = test_support_init_keyset_slots(
                    this->_requested_keyset_size,
                    KEYSET_GENERATOR_METHOD,
                    544498304);

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

        // Set up the worker context, as it's required by the storage db, this has to be done here as the worker_context
        // is stored in a thread variable an the threads are managed internally by the benchmarking library and therefore
        // they can be recycled or re-created.
        worker_context_t *worker_context;
        if ((worker_context = worker_context_get()) == nullptr) {
            // This assigned memory will be lost but this is a benchmark and we don't care
            worker_context = (worker_context_t *) ffma_mem_alloc(sizeof(worker_context_t));
            worker_context_set(worker_context);
        }

        // Setup the worker as needed
        worker_context->worker_index = state.thread_index();
        worker_context->workers_count = this->GetWorkersCount();
        worker_context->db = (storage_db *) static_db;

        if (state.thread_index() == 0) {
            fprintf(stdout, "> Setup - populating storage_db\n");
            fflush(stdout);
        }

        for(
                uint64_t key_index = state.thread_index();
                key_index < this->_requested_keyset_size;
                key_index += state.threads()) {
            bool result = storage_db_set_small_value(
                    (storage_db*)static_db,
                    static_keyset_slots[key_index].key,
                    static_keyset_slots[key_index].key_length,
                    this->_value_buffer,
                    this->_value_buffer_length);

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

            fprintf(stdout, "> Setup - second keyset slots generation\n");
            fflush(stdout);

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

        this->_db = (storage_db*)static_db;
        this->_workers_count = state.threads();
        this->_keyset_slots = (test_support_keyset_slot_t *)static_keyset_slots;

        if (state.thread_index() == 0) {
            fprintf(stdout, "> Setup - completed\n");
            fflush(stdout);
        }
    }

    void TearDown(const ::benchmark::State& state) override {

        if (state.thread_index() != 0) {
            return;
        }

        fprintf(stdout, "< Teardown - started\n");
        fflush(stdout);

        if (this->_db != nullptr) {
            fprintf(stdout, "< Teardown - collecting hashtable statistics\n");
            fflush(stdout);

            BenchmarkSupport::CollectHashtableStatsAndUpdateState(
                    (benchmark::State&)state, this->_db->hashtable);

            fprintf(stdout, "< Teardown - cleaning up storage_db\n");
            fflush(stdout);

            // Free the storage
            storage_db_free(this->_db, this->_workers_count);
        }

        if (this->_keyset_slots != nullptr) {
            fprintf(stdout, "< Teardown - free-ing up the keyset slots\n");
            fflush(stdout);

            // Free the keys
            test_support_free_keyset_slots(this->_keyset_slots);
        }

        this->_db = nullptr;
        this->_workers_count = 0;
        this->_keyset_slots = nullptr;
        this->_requested_keyset_size = 0;

        static_db = nullptr;
        static_keyset_slots = nullptr;
        static_storage_db_populated = false;

        fprintf(stdout, "< Teardown - completed\n");
        fflush(stdout);
    }
};

BENCHMARK_DEFINE_F(StorageDbOpGetFixture, storage_db_op_get_different_keys)(benchmark::State& state) {
    uint64_t requested_keyset_size;
    test_support_keyset_slot_t *keyset_slots;
    worker_context_t *worker_context;
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
    requested_keyset_size = this->GetRequestedKeysetSize();

    for (auto _ : state) {
        for(
                uint64_t key_index = state.thread_index();
                key_index < requested_keyset_size;
                key_index += state.threads()) {
            storage_db_entry_index_t *entry_index = storage_db_get_entry_index(
                    worker_context->db,
                    keyset_slots[key_index].key,
                    keyset_slots[key_index].key_length);

            if (unlikely(!entry_index)) {
                sprintf(
                        error_message,
                        "Can't find the key <%s (%d)> with index <%ld> for the thread <%d>",
                        keyset_slots[key_index].key,
                        keyset_slots[key_index].key_length,
                        key_index,
                        state.thread_index());
                state.SkipWithError(error_message);
                break;
            }

            if (likely(entry_index)) {
                storage_db_entry_index_status_t old_status;

                // Try to acquire a reader lock until it's successful or the entry index has been marked as deleted
                storage_db_entry_index_status_increase_readers_counter(
                        entry_index,
                        &old_status);

                if (unlikely(old_status.deleted)) {
                    sprintf(
                            error_message,
                            "The key <%s (%d)> with index <%ld> for the thread <%d> has been deleted but it's not possible!",
                            keyset_slots[key_index].key,
                            keyset_slots[key_index].key_length,
                            key_index,
                            state.thread_index());
                    state.SkipWithError(error_message);
                    break;
                }
            }

            // Only <= 64kb values so no need to iterate over the chunks
            storage_db_chunk_info_t *chunk_info = storage_db_entry_value_chunk_get(entry_index, 0);

            char *buffer;

            if (likely(storage_db_entry_chunk_can_read_from_memory(worker_context->db, chunk_info))) {
                benchmark::DoNotOptimize((buffer = storage_db_entry_chunk_read_fast_from_memory(worker_context->db, chunk_info)));
            } else {
                sprintf(
                        error_message,
                        "Can't perform fast read from memory for key <%s (%d)> with index <%ld> for the thread <%d>",
                        keyset_slots[key_index].key,
                        keyset_slots[key_index].key_length,
                        key_index,
                        state.thread_index());
                state.SkipWithError(error_message);
                break;
            }

            storage_db_entry_index_status_decrease_readers_counter(entry_index, nullptr);
        }
    }
}


BENCHMARK_DEFINE_F(StorageDbOpGetFixture, storage_db_op_get_same_keys)(benchmark::State& state) {
    uint64_t requested_keyset_size;
    test_support_keyset_slot_t *keyset_slots;
    worker_context_t *worker_context;
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
    requested_keyset_size = this->GetRequestedKeysetSize();

    for (auto _ : state) {
        for(
                uint64_t key_index = 0;
                key_index < requested_keyset_size;
                key_index++) {
            storage_db_entry_index_t *entry_index = storage_db_get_entry_index(
                    worker_context->db,
                    keyset_slots[key_index].key,
                    keyset_slots[key_index].key_length);

            if (unlikely(!entry_index)) {
                sprintf(
                        error_message,
                        "Can't find the key <%s (%d)> with index <%ld> for the thread <%d>",
                        keyset_slots[key_index].key,
                        keyset_slots[key_index].key_length,
                        key_index,
                        state.thread_index());
                state.SkipWithError(error_message);
                break;
            }

            if (likely(entry_index)) {
                storage_db_entry_index_status_t old_status;

                // Try to acquire a reader lock until it's successful or the entry index has been marked as deleted
                storage_db_entry_index_status_increase_readers_counter(
                        entry_index,
                        &old_status);

                if (unlikely(old_status.deleted)) {
                    sprintf(
                            error_message,
                            "The key <%s (%d)> with index <%ld> for the thread <%d> has been deleted but it's not possible!",
                            keyset_slots[key_index].key,
                            keyset_slots[key_index].key_length,
                            key_index,
                            state.thread_index());
                    state.SkipWithError(error_message);
                    break;
                }
            }

            // Only <= 64kb values so no need to iterate over the chunks
            storage_db_chunk_info_t *chunk_info = storage_db_entry_value_chunk_get(entry_index, 0);

            char *buffer;

            if (likely(storage_db_entry_chunk_can_read_from_memory(worker_context->db, chunk_info))) {
                benchmark::DoNotOptimize((buffer = storage_db_entry_chunk_read_fast_from_memory(worker_context->db, chunk_info)));
            } else {
                sprintf(
                        error_message,
                        "Can't perform fast read from memory for key <%s (%d)> with index <%ld> for the thread <%d>",
                        keyset_slots[key_index].key,
                        keyset_slots[key_index].key_length,
                        key_index,
                        state.thread_index());
                state.SkipWithError(error_message);
                break;
            }

            storage_db_entry_index_status_decrease_readers_counter(entry_index, nullptr);
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

BENCHMARK_REGISTER_F(StorageDbOpGetFixture, storage_db_op_get_different_keys)
        ->Apply(BenchArguments);


BENCHMARK_REGISTER_F(StorageDbOpGetFixture, storage_db_op_get_same_keys)
        ->Apply(BenchArguments);
