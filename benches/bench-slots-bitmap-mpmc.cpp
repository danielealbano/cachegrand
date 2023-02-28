/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

//Run on (32 X 4200.07 MHz CPU s)
//CPU Caches:
//L1 Data 32 KiB (x16)
//L1 Instruction 32 KiB (x16)
//L2 Unified 512 KiB (x16)
//L3 Unified 16384 KiB (x4)
//Load Average: 0.82, 1.41, 2.10
//--------------------------------------------------------------------------------------------------------------------------
//Benchmark                                                                                Time             CPU   Iterations
//--------------------------------------------------------------------------------------------------------------------------
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:1_mean             235 ns          235 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:1_median           231 ns          231 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:1_stddev          12.2 ns         12.1 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:1_cv              5.18 %          5.15 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:2_mean             108 ns          216 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:2_median           108 ns          215 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:2_stddev          1.25 ns         2.51 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:2_cv              1.16 %          1.16 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:4_mean            47.8 ns          191 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:4_median          47.7 ns          191 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:4_stddev         0.621 ns         2.46 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:4_cv              1.30 %          1.29 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:8_mean            18.1 ns          145 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:8_median          18.0 ns          144 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:8_stddev         0.244 ns         1.96 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:8_cv              1.35 %          1.35 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:16_mean           7.31 ns          117 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:16_median         7.29 ns          117 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:16_stddev        0.097 ns         1.55 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:16_cv             1.32 %          1.32 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:32_mean           3.53 ns          112 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:32_median         3.51 ns          112 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:32_stddev        0.117 ns        0.534 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:32_cv             3.31 %          0.48 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:1_mean           230 ns          230 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:1_median         228 ns          228 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:1_stddev        7.43 ns         7.38 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:1_cv            3.23 %          3.21 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:2_mean           212 ns          424 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:2_median         212 ns          424 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:2_stddev        1.36 ns         2.71 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:2_cv            0.64 %          0.64 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:4_mean           187 ns          748 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:4_median         187 ns          747 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:4_stddev       0.720 ns         2.86 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:4_cv            0.38 %          0.38 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:8_mean           140 ns         1122 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:8_median         140 ns         1119 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:8_stddev       0.631 ns         5.05 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:8_cv            0.45 %          0.45 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:16_mean         74.1 ns         1185 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:16_median       73.2 ns         1171 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:16_stddev       1.99 ns         31.7 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:16_cv           2.68 %          2.67 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:32_mean         49.1 ns         1549 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:32_median       48.9 ns         1551 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:32_stddev      0.635 ns         4.74 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:32_cv           1.29 %          0.31 %            50

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdbool>

#include <benchmark/benchmark.h>

#include "misc.h"
#include "memory_fences.h"
#include "exttypes.h"
#include "utils_cpu.h"
#include "thread.h"

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

    thread_current_set_affinity(state.thread_index());

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

    thread_current_set_affinity(state.thread_index());

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
