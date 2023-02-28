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
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:1_mean             214 ns          214 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:1_median           213 ns          213 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:1_stddev          2.35 ns         2.35 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:1_cv              1.10 %          1.10 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:2_mean             101 ns          199 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:2_median           101 ns          199 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:2_stddev         0.552 ns        0.944 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:2_cv              0.55 %          0.47 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:4_mean            45.2 ns          176 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:4_median          45.1 ns          176 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:4_stddev         0.239 ns        0.868 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:4_cv              0.53 %          0.49 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:8_mean            17.3 ns          133 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:8_median          17.3 ns          133 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:8_stddev         0.127 ns        0.714 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:8_cv              0.73 %          0.53 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:16_mean           11.4 ns          108 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:16_median         11.4 ns          108 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:16_stddev        0.474 ns        0.478 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:16_cv             4.14 %          0.44 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:32_mean           8.95 ns          101 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:32_median         8.89 ns          101 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:32_stddev        0.227 ns        0.589 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:32_cv             2.53 %          0.58 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:1_mean           210 ns          210 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:1_median         210 ns          210 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:1_stddev       0.358 ns        0.250 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:1_cv            0.17 %          0.12 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:2_mean           297 ns          390 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:2_median         309 ns          390 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:2_stddev        31.5 ns        0.777 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:2_cv           10.59 %          0.20 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:4_mean           555 ns          687 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:4_median         556 ns          686 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:4_stddev        16.4 ns         1.07 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:4_cv            2.96 %          0.16 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:8_mean           946 ns         1027 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:8_median         947 ns         1027 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:8_stddev        5.93 ns        0.604 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:8_cv            0.63 %          0.06 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:16_mean          992 ns         1073 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:16_median        991 ns         1072 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:16_stddev       3.98 ns         3.33 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:16_cv           0.40 %          0.31 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:32_mean         1002 ns         1075 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:32_median       1001 ns         1075 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:32_stddev       3.53 ns         2.82 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:32_cv           0.35 %          0.26 %            50

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
