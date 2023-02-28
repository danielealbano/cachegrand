/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdbool>

#include <benchmark/benchmark.h>

#include "exttypes.h"
#include "utils_cpu.h"

#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"

#include "benchmark-program-simple.hpp"

// It is possible to control the amount of threads used for the test tuning the two defines below
#define TEST_THREADS_RANGE_BEGIN (1)
#define TEST_THREADS_RANGE_END (utils_cpu_count())

static void slots_bitmap_mpmc_fill_parallel(benchmark::State& state) {
    int current_thread = state.thread_index();
    int total_threads = state.threads();
    const uint64_t size = ((sizeof(uint64_t) * 8) * (64 / sizeof(uint64_t))) * total_threads * 10;
    uint64_t start = (size / total_threads) * current_thread;
    slots_bitmap_mpmc_t *bitmap = slots_bitmap_mpmc_init(size);

    for (auto _ : state) {
        benchmark::DoNotOptimize(slots_bitmap_mpmc_get_next_available_with_step(bitmap, start, 1));
    }

    slots_bitmap_mpmc_free(bitmap);
}

static void slots_bitmap_mpmc_fill_sequential(benchmark::State& state) {
    int current_thread = state.thread_index();
    int total_threads = state.threads();
    const uint64_t size = ((sizeof(uint64_t) * 8) * (64 / sizeof(uint64_t))) * total_threads * 10;
    slots_bitmap_mpmc_t *bitmap = slots_bitmap_mpmc_init(size);

    for (auto _ : state) {
        benchmark::DoNotOptimize(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 1, 1));
    }

    slots_bitmap_mpmc_free(bitmap);
}

static void BenchArguments(benchmark::internal::Benchmark* b) {
    // To run more than 131072 iterations is necessary to increase EPOCH_OPERATION_QUEUE_RING_SIZE in
    // epoch_operations_queue.h as there is no processing of the queue included with the test
    b
            ->ThreadRange(TEST_THREADS_RANGE_BEGIN, TEST_THREADS_RANGE_END)
            ->Iterations(((sizeof(uint64_t) * 8) * (64 / sizeof(uint64_t))) * 100)
            ->Repetitions(50)
            ->DisplayAggregatesOnly(true);
}

BENCHMARK(slots_bitmap_mpmc_fill_parallel)
    ->Apply(BenchArguments);

BENCHMARK(slots_bitmap_mpmc_fill_sequential)
    ->Apply(BenchArguments);
