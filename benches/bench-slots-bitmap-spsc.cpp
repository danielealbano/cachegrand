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

#include "data_structures/slots_bitmap_spsc/slots_bitmap_spsc.h"

#include "benchmark-program-simple.hpp"

// It is possible to control the amount of threads used for the test tuning the two defines below
#define TEST_THREADS_RANGE_BEGIN (1)
#define TEST_THREADS_RANGE_END (utils_cpu_count())

static void slots_bitmap_spsc_fill_sequential(benchmark::State& state) {
    int current_thread = state.thread_index();
    int total_threads = state.threads();
    const uint64_t size = ((sizeof(uint64_t) * 8) * (64 / sizeof(uint64_t))) * 10;
    slots_bitmap_spsc_t *bitmap = slots_bitmap_spsc_init(size);

    for (auto _ : state) {
        benchmark::DoNotOptimize(slots_bitmap_spsc_get_next_available(bitmap));
    }

    slots_bitmap_spsc_free(bitmap);
}

static void BenchArguments(benchmark::internal::Benchmark* b) {
    // To run more than 131072 iterations is necessary to increase EPOCH_OPERATION_QUEUE_RING_SIZE in
    // epoch_operations_queue.h as there is no processing of the queue included with the test
    b
            ->Iterations(((sizeof(uint64_t) * 8) * (64 / sizeof(uint64_t))) * 100)
            ->Repetitions(50)
            ->DisplayAggregatesOnly(true);
}

BENCHMARK(slots_bitmap_spsc_fill_sequential)
    ->Apply(BenchArguments);
