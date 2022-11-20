/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <cstdio>
#include <cstring>

#include <benchmark/benchmark.h>

#include "misc.h"
#include "exttypes.h"
#include "xalloc.h"
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
#include "fiber/fiber.h"
#include "fiber/fiber_scheduler.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "epoch_gc.h"
#include "data_structures/hashtable_mpmc/hashtable_mpmc.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"

#include "../tests/unit_tests/support.h"
#include "../tests/unit_tests/data_structures/hashtable/mpmc/fixtures-hashtable-mpmc.h"

#include "log/log.h"
#include "log/sink/log_sink.h"
#include "log/sink/log_sink_console.h"

#define TEST_VALIDATE_KEYS 0

// It is possible to control the amount of threads used for the test tuning the two defines below
#define TEST_THREADS_RANGE_BEGIN (1)
#define TEST_THREADS_RANGE_END (utils_cpu_count())

class BenchmarkProgram {
private:
    const char* tag;

    void setup_initial_log_sink_console() {
        log_level_t level = LOG_LEVEL_ALL;
        log_sink_settings_t settings = { 0 };
        settings.console.use_stdout_for_errors = false;

        log_sink_register(log_sink_console_init(level, &settings));
    }

public:
    explicit BenchmarkProgram(const char *tag) {
        this->tag = tag;
    }

    int Main(int argc, char** argv) {
        // Setup the log sink
        BenchmarkProgram::setup_initial_log_sink_console();

        // Ensure that the current thread is pinned to the core 0 otherwise some tests can fail if the kernel shift around
        // the main thread of the process
        thread_current_set_affinity(0);

        ::benchmark::Initialize(&argc, argv);
        if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
            return 1;
        }
        ::benchmark::RunSpecifiedBenchmarks();

        return 0;
    }
};

int main(int argc, char** argv) {
    return BenchmarkProgram(__FILE__).Main(argc, argv);
}

static void hashtable_mpmc_op_get_not_found_key(benchmark::State& state) {
    static hashtable_mpmc_t * hashtable;
    hashtable_value_data_t value;
    worker_context_t worker_context = { 0 };

    worker_context.worker_index = state.thread_index();
    worker_context_set(&worker_context);
    transaction_set_worker_index(worker_context.worker_index);

    hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_init();

    if (state.thread_index() == 0) {
        hashtable = hashtable_mpmc_init(state.range(0));
    }

    test_support_set_thread_affinity(state.thread_index());

    for (auto _ : state) {
        benchmark::DoNotOptimize(hashtable_mpmc_op_get(
                hashtable,
                test_key_1,
                test_key_1_len,
                &value));
    }

    if (state.thread_index() == 0) {
        hashtable_mpmc_free(hashtable);
    }

    hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free();
}

static void hashtable_mpmc_op_get_single_key_inline(benchmark::State& state) {
    static hashtable_mpmc_t * hashtable;
    static hashtable_mpmc_bucket_index_t bucket_index;
    uintptr_t value;
    bool result;
    char error_message[150] = {0};
    worker_context_t worker_context = { 0 };

    worker_context.worker_index = state.thread_index();
    worker_context_set(&worker_context);
    transaction_set_worker_index(worker_context.worker_index);

    hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_init();

    if (state.thread_index() == 0) {
        hashtable = hashtable_mpmc_init(state.range(0));

        bucket_index = hashtable_mpmc_support_bucket_index_from_hash(
                hashtable->data,
                test_key_1_hash);
        char *test_key_1_clone = (char*)xalloc_alloc(test_key_1_len + 1);
        strncpy(test_key_1_clone, test_key_1, test_key_1_len);

        auto key_value = (hashtable_mpmc_data_key_value_t*)xalloc_alloc(sizeof(hashtable_mpmc_data_key_value_t));
        strncpy(key_value->key.embedded.key, test_key_1_clone, test_key_1_len);
        key_value->key.embedded.key_length = test_key_1_len;
        key_value->value = test_value_1;
        key_value->hash = test_key_1_hash;
        key_value->key_is_embedded = true;

        hashtable->data->buckets[bucket_index].data.hash_half = hashtable_mpmc_support_hash_half(test_key_1_hash);
        hashtable->data->buckets[bucket_index].data.key_value = key_value;
    }

    test_support_set_thread_affinity(state.thread_index());

    for (auto _ : state) {
        benchmark::DoNotOptimize((result = hashtable_mpmc_op_get(
                hashtable,
                test_key_1,
                test_key_1_len,
                &value)));

#if TEST_VALIDATE_KEYS == 1
        if (result == HASHTABLE_MPMC_RESULT_FALSE || value != test_value_1) {
            sprintf(
                    error_message,
                    "Unable to get the key <%s> with bucket index <%lu> for the thread <%d>",
                    test_key_1,
                    bucket_index,
                    state.thread_index());
            state.SkipWithError(error_message);
            break;
        }
#endif
    }

    if (state.thread_index() == 0) {
        hashtable_mpmc_free(hashtable);
    }

    hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free();
}

static void hashtable_mpmc_op_get_single_key_external(benchmark::State& state) {
    static hashtable_mpmc_t * hashtable;
    static hashtable_mpmc_bucket_index_t bucket_index;
    uintptr_t value;
    bool result;
    char error_message[150] = {0};
    worker_context_t worker_context = { 0 };

    worker_context.worker_index = state.thread_index();
    worker_context_set(&worker_context);
    transaction_set_worker_index(worker_context.worker_index);

    hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_init();

    if (state.thread_index() == 0) {
        hashtable = hashtable_mpmc_init(state.range(0));

        bucket_index = hashtable_mpmc_support_bucket_index_from_hash(
                hashtable->data,
                test_key_1_hash);
        char *test_key_1_clone = (char*)xalloc_alloc(test_key_1_len + 1);
        strncpy(test_key_1_clone, test_key_1, test_key_1_len);

        auto key_value = (hashtable_mpmc_data_key_value_t*)xalloc_alloc(sizeof(hashtable_mpmc_data_key_value_t));
        key_value->key.external.key = test_key_1_clone;
        key_value->key.external.key_length = test_key_1_len;
        key_value->value = test_value_1;
        key_value->hash = test_key_1_hash;
        key_value->key_is_embedded = false;

        hashtable->data->buckets[bucket_index].data.hash_half = hashtable_mpmc_support_hash_half(test_key_1_hash);
        hashtable->data->buckets[bucket_index].data.key_value = key_value;
    }

    test_support_set_thread_affinity(state.thread_index());

    for (auto _ : state) {
        benchmark::DoNotOptimize((result = hashtable_mpmc_op_get(
                hashtable,
                test_key_1,
                test_key_1_len,
                &value)));

#if TEST_VALIDATE_KEYS == 1
        if (result == HASHTABLE_MPMC_RESULT_FALSE || value != test_value_1) {
            sprintf(
                    error_message,
                    "Unable to get the key <%s> with bucket index <%lu> for the thread <%d>",
                    test_key_1,
                    bucket_index,
                    state.thread_index());
            state.SkipWithError(error_message);
            break;
        }
#endif
    }

    if (state.thread_index() == 0) {
        hashtable_mpmc_free(hashtable);
    }

    hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free();
}

static void BenchArguments(benchmark::internal::Benchmark* b) {
    // To run more than 131072 iterations is necessary to increase EPOCH_OPERATION_QUEUE_RING_SIZE in
    // epoch_operations_queue.h as there is no processing of the queue included with the test
    b
            ->Arg(256)
            ->ThreadRange(TEST_THREADS_RANGE_BEGIN, TEST_THREADS_RANGE_END)
            ->Iterations(131072)
            ->DisplayAggregatesOnly(false);
}

BENCHMARK(hashtable_mpmc_op_get_not_found_key)
        ->Apply(BenchArguments);

BENCHMARK(hashtable_mpmc_op_get_single_key_inline)
        ->Apply(BenchArguments);

BENCHMARK(hashtable_mpmc_op_get_single_key_external)
        ->Apply(BenchArguments);
