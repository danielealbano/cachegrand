/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

//Run on (32 X 4199.98 MHz CPU s)
//CPU Caches:
//L1 Data 32 KiB (x16)
//L1 Instruction 32 KiB (x16)
//L2 Unified 512 KiB (x16)
//L3 Unified 16384 KiB (x4)
//Load Average: 14.45, 7.05, 3.46
//---------------------------------------------------------------------------------------------------------------
//Benchmark                                                                     Time             CPU   Iterations
//---------------------------------------------------------------------------------------------------------------
//slots_bitmap_spsc_fill_sequential/iterations:51200/repeats:50_mean          181 ns          181 ns           50
//slots_bitmap_spsc_fill_sequential/iterations:51200/repeats:50_median        178 ns          178 ns           50
//slots_bitmap_spsc_fill_sequential/iterations:51200/repeats:50_stddev       8.33 ns         8.35 ns           50
//slots_bitmap_spsc_fill_sequential/iterations:51200/repeats:50_cv           4.60 %          4.62 %            50

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
