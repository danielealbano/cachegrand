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
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:1_mean            86.9 ns         86.9 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:1_median          87.2 ns         87.2 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:1_stddev         0.971 ns        0.967 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:1_cv              1.12 %          1.11 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:2_mean            66.2 ns          132 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:2_median          72.0 ns          144 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:2_stddev          12.4 ns         24.7 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:2_cv             18.66 %         18.66 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:4_mean            17.4 ns         69.4 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:4_median          17.0 ns         67.8 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:4_stddev          1.12 ns         4.49 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:4_cv              6.46 %          6.47 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:8_mean            6.84 ns         54.6 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:8_median          6.52 ns         52.1 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:8_stddev          1.04 ns         8.23 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:8_cv             15.20 %         15.07 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:16_mean           2.67 ns         42.6 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:16_median         2.64 ns         42.2 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:16_stddev        0.104 ns         1.67 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:16_cv             3.91 %          3.91 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:32_mean           1.35 ns         43.0 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:32_median         1.33 ns         42.5 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:32_stddev        0.075 ns         2.30 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:32_cv             5.59 %          5.36 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:1_mean          85.8 ns         85.8 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:1_median        85.9 ns         85.8 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:1_stddev       0.940 ns        0.930 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:1_cv            1.09 %          1.08 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:2_mean          74.5 ns          149 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:2_median        74.4 ns          149 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:2_stddev       0.234 ns        0.475 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:2_cv            0.31 %          0.32 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:4_mean          64.3 ns          257 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:4_median        64.2 ns          257 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:4_stddev       0.297 ns         1.19 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:4_cv            0.46 %          0.46 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:8_mean          48.2 ns          386 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:8_median        48.1 ns          384 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:8_stddev       0.553 ns         4.38 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:8_cv            1.15 %          1.14 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:16_mean         25.1 ns          401 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:16_median       25.0 ns          400 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:16_stddev      0.187 ns         2.94 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:16_cv           0.75 %          0.73 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:32_mean         24.7 ns          779 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:32_median       24.6 ns          781 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:32_stddev      0.197 ns         3.32 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:32_cv           0.80 %          0.43 %            50

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
