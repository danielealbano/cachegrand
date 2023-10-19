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
//slots_bitmap_spsc_fill_sequential/iterations:51200/repeats:50_mean          877 ns          877 ns           50
//slots_bitmap_spsc_fill_sequential/iterations:51200/repeats:50_median        876 ns          876 ns           50
//slots_bitmap_spsc_fill_sequential/iterations:51200/repeats:50_stddev       3.17 ns         3.16 ns           50
//slots_bitmap_spsc_fill_sequential/iterations:51200/repeats:50_cv           0.36 %          0.36 %            50

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdbool>

#include <benchmark/benchmark.h>

#include "exttypes.h"

#include "data_structures/slots_bitmap_spsc/slots_bitmap_spsc.h"

#include "benchmark-program-simple.hpp"

#define BITS_PER_THREAD (((sizeof(uint64_t) * 8) * (64 / sizeof(uint64_t))) * 100)

static void slots_bitmap_spsc_fill_sequential(benchmark::State& state) {
    const uint64_t size = BITS_PER_THREAD;
    slots_bitmap_spsc_t *bitmap = slots_bitmap_spsc_init(size);

    for (auto _ : state) {
        benchmark::DoNotOptimize(slots_bitmap_spsc_get_next_available(bitmap));
    }

    slots_bitmap_spsc_free(bitmap);
}

static void BenchArguments(benchmark::internal::Benchmark* b) {
    b
            ->Iterations(BITS_PER_THREAD)
            ->Repetitions(50)
            ->DisplayAggregatesOnly(true);
}

BENCHMARK(slots_bitmap_spsc_fill_sequential)
    ->Apply(BenchArguments);
