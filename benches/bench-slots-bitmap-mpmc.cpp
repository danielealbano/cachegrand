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
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:1_mean                        296 ns          296 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:1_median                      285 ns          285 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:1_stddev                     43.9 ns         43.9 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:1_cv                        14.84 %         14.85 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:2_mean                       75.8 ns          152 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:2_median                     72.2 ns          144 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:2_stddev                     12.4 ns         24.9 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:2_cv                        16.39 %         16.40 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:4_mean                       18.7 ns         74.8 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:4_median                     18.6 ns         74.4 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:4_stddev                    0.412 ns         1.65 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:4_cv                         2.20 %          2.20 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:8_mean                       4.97 ns         39.7 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:8_median                     4.95 ns         39.6 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:8_stddev                    0.105 ns        0.837 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:8_cv                         2.10 %          2.11 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:16_mean                      4.70 ns         75.2 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:16_median                    4.70 ns         75.2 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:16_stddev                   0.021 ns        0.337 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:16_cv                        0.44 %          0.45 %            50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:32_mean                      3.85 ns          123 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:32_median                    3.84 ns          123 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:32_stddev                   0.034 ns         1.10 ns           50
//slots_bitmap_mpmc_fill_parallel/iterations:51200/repeats:50/threads:32_cv                        0.89 %          0.89 %            50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:1_mean      15224414 ns     15221858 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:1_median    15099228 ns     15096381 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:1_stddev      695354 ns       694733 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:1_cv            4.57 %          4.56 %            50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:2_mean       1027646 ns      2054940 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:2_median     1027204 ns      2054054 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:2_stddev        2107 ns         4188 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:2_cv            0.21 %          0.20 %            50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:4_mean         74484 ns       297899 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:4_median       74470 ns       297824 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:4_stddev         151 ns          606 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:4_cv            0.20 %          0.20 %            50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:8_mean          5854 ns        46821 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:8_median        5860 ns        46846 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:8_stddev        41.6 ns          331 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:8_cv            0.71 %          0.71 %            50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:16_mean          497 ns         7940 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:16_median        499 ns         7978 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:16_stddev       9.04 ns          145 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:16_cv           1.82 %          1.82 %            50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:32_mean         42.1 ns         1340 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:32_median       42.0 ns         1340 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:32_stddev      0.192 ns         5.79 ns           50
//slots_bitmap_mpmc_fill_all_and_release_parallel/iterations:50/repeats:50/threads:32_cv           0.46 %          0.43 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:1_mean                      285 ns          285 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:1_median                    285 ns          285 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:1_stddev                  0.968 ns        0.959 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:1_cv                       0.34 %          0.34 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:2_mean                      301 ns          601 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:2_median                    301 ns          601 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:2_stddev                   1.09 ns         2.18 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:2_cv                       0.36 %          0.36 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:4_mean                      294 ns         1175 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:4_median                    289 ns         1157 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:4_stddev                   7.48 ns         29.8 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:4_cv                       2.55 %          2.54 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:8_mean                      310 ns         2476 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:8_median                    309 ns         2473 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:8_stddev                   2.73 ns         20.9 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:8_cv                       0.88 %          0.84 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:16_mean                     300 ns         4802 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:16_median                   300 ns         4792 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:16_stddev                  2.75 ns         43.2 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:16_cv                      0.92 %          0.90 %            50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:32_mean                     560 ns        17623 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:32_median                   558 ns        17662 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:32_stddev                  5.24 ns          156 ns           50
//slots_bitmap_mpmc_fill_sequential/iterations:51200/repeats:50/threads:32_cv                      0.94 %          0.88 %            50

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
#define BITS_PER_THREAD (((sizeof(uint64_t) * 8) * (64 / sizeof(uint64_t))) * 100)

static slots_bitmap_mpmc_t *bitmap = nullptr;

static void slots_bitmap_mpmc_fill_parallel(benchmark::State& state) {
    int current_thread = state.thread_index();
    int total_threads = state.threads();
    const uint64_t size_per_thread = BITS_PER_THREAD;
    const uint64_t size = size_per_thread * total_threads;
    uint64_t start = (size / total_threads) * current_thread;

    if (current_thread == 0) {
        bitmap = slots_bitmap_mpmc_init(size);
    }

    thread_current_set_affinity(state.thread_index());

    for (auto _ : state) {
        benchmark::DoNotOptimize(slots_bitmap_mpmc_get_next_available_with_step(bitmap, start, 1));
    }

    if (current_thread == 0) {
        slots_bitmap_mpmc_free(bitmap);
        bitmap = nullptr;
    }
}

static void slots_bitmap_mpmc_fill_all_and_release_parallel(benchmark::State& state) {
    int current_thread = state.thread_index();
    int total_threads = state.threads();
    const uint64_t size_per_thread = BITS_PER_THREAD;
    const uint64_t size = size_per_thread * total_threads;
    uint64_t start = (size / total_threads) * current_thread;

    if (current_thread == 0) {
        bitmap = slots_bitmap_mpmc_init(size);
    }

    thread_current_set_affinity(state.thread_index());

    for (auto _ : state) {
        for(uint64_t i = start; i < size_per_thread / total_threads; i++) {
            benchmark::DoNotOptimize(slots_bitmap_mpmc_get_next_available_with_step(bitmap, start, 1));
        }

        for(uint64_t i = start; i < size_per_thread / total_threads; i++) {
            slots_bitmap_mpmc_release(bitmap, i);
        }
    }

    if (current_thread == 0) {
        slots_bitmap_mpmc_free(bitmap);
        bitmap = nullptr;
    }
}

static void slots_bitmap_mpmc_fill_sequential(benchmark::State& state) {
    int current_thread = state.thread_index();
    int total_threads = state.threads();
    const uint64_t size_per_thread = BITS_PER_THREAD;
    const uint64_t size = size_per_thread * total_threads;

    if (current_thread == 0) {
        bitmap = slots_bitmap_mpmc_init(size);
    }

    thread_current_set_affinity(state.thread_index());

    for (auto _ : state) {
        benchmark::DoNotOptimize(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 1, 1));
    }

    if (current_thread == 0) {
        slots_bitmap_mpmc_free(bitmap);
        bitmap = nullptr;
    }
}

static void BenchArguments(benchmark::internal::Benchmark* b) {
    b
            ->ThreadRange(TEST_THREADS_RANGE_BEGIN, TEST_THREADS_RANGE_END)
            ->Iterations(BITS_PER_THREAD)
            ->Repetitions(50)
            ->DisplayAggregatesOnly(true);
}

BENCHMARK(slots_bitmap_mpmc_fill_parallel)
->Apply(BenchArguments);

BENCHMARK(slots_bitmap_mpmc_fill_all_and_release_parallel)
    ->Apply(BenchArguments)->Iterations(50);

BENCHMARK(slots_bitmap_mpmc_fill_sequential)
    ->Apply(BenchArguments);
