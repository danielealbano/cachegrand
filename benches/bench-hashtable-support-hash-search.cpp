/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <cstdlib>
#include <cstdint>
#include <cstdint>
#include <immintrin.h>
#include <numa.h>

#include <benchmark/benchmark.h>

#include "exttypes.h"
#include "spinlock.h"

#include "data_structures/hashtable/mcmp/hashtable.h"

#if defined(__x86_64__)
#if CACHEGRAND_CMAKE_CONFIG_HOST_HAS_AVX512F == 1
#include "data_structures/hashtable/mcmp/hashtable_support_hash_search_avx512f.h"
#endif
#if CACHEGRAND_CMAKE_CONFIG_HOST_HAS_AVX2 == 1
#include "data_structures/hashtable/mcmp/hashtable_support_hash_search_avx2.h"
#endif
#if CACHEGRAND_CMAKE_CONFIG_HOST_HAS_AVX == 1
#include "data_structures/hashtable/mcmp/hashtable_support_hash_search_avx.h"
#endif
#endif

#include "data_structures/hashtable/mcmp/hashtable_support_hash_search_loop.h"

#include "../tests/support.h"

#include "benchmark-program.hpp"
#include "benchmark-support.hpp"

uint32_t* init_hashes() {
    uint32_t* __attribute__((aligned(16))) hashes =
            (uint32_t*)aligned_alloc(16, sizeof(uint32_t) * 16);

    for(uint32_t i = 0; i < 16; i++) {
        hashes[i] = i;
    }
    hashes[14] = 0;
    hashes[15] = 0;

    return hashes;
}

#define BENCH_TEMPLATE_HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_FUNC_WRAPPER(METHOD, SUFFIX, ...) \
    void bench_template_hashtable_mcmp_support_hash_search_##METHOD##_##SUFFIX(benchmark::State& state) { \
        uint32_t* hashes = NULL; \
        \
        test_support_set_thread_affinity(state.thread_index()); \
        \
        for (auto _ : state) { \
            hashes = init_hashes(); \
            __VA_ARGS__ \
            free(hashes); \
        } \
    } \
    BENCHMARK(bench_template_hashtable_mcmp_support_hash_search_##METHOD##_##SUFFIX) \
        ->Iterations(10000000) \
        ->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(16)->Threads(32)->Threads(64)->Threads(128);

#define BENCH_TEMPLATE_HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_FULL(METHOD) \
    BENCH_TEMPLATE_HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_FUNC_WRAPPER(METHOD, full, { \
        volatile uint32_t skip_hashes = 0x04 | 0x100 | 0x400; \
        hashtable_hash_half_volatile_t hashes[] =  { 8, 1, 13, 4, 9, 0, 5, 11, 3, 12, 7, 2, 15, 14, 6, 10 }; \
        for(uint8_t i = 0; i < 13; i++) { \
            benchmark::DoNotOptimize( \
                hashtable_mcmp_support_hash_search_##METHOD##_14(hashes[i], hashes, skip_hashes) \
                ); \
        } \
    })

#define BENCH_TEMPLATE_HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_ALL(METHOD) \
    BENCH_TEMPLATE_HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_FULL(METHOD);

BENCH_TEMPLATE_HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_ALL(loop);
BENCH_TEMPLATE_HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_ALL(avx);
BENCH_TEMPLATE_HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_ALL(avx2);
