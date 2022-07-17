/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <random>
#include <algorithm>
#include <iterator>

#include <cstring>
#include <benchmark/benchmark.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "thread.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "slab_allocator.h"

#include "benchmark-program.hpp"
#include "benchmark-support.hpp"

// About (64kb * 4096 * 48 / 2048) = 6144 hugepages are required to run the test with the slab allocator and about 12GB
// are required to run the test with malloc.

#define SET_BENCH_ARGS() \
      Iterations(1) \
    ->Repetitions(25) \
    ->DisplayAggregatesOnly(true) \
    ->ArgsProduct({ \
        { SLAB_OBJECT_SIZE_16, SLAB_OBJECT_SIZE_32, SLAB_OBJECT_SIZE_64, SLAB_OBJECT_SIZE_128, SLAB_OBJECT_SIZE_256, \
          SLAB_OBJECT_SIZE_512, SLAB_OBJECT_SIZE_1024, SLAB_OBJECT_SIZE_2048, SLAB_OBJECT_SIZE_4096, \
          SLAB_OBJECT_SIZE_8192, SLAB_OBJECT_SIZE_16384, SLAB_OBJECT_SIZE_32768, SLAB_OBJECT_SIZE_65536 }, \
        { 0x1000 } \
    })

#define SET_BENCH_THREADS() \
    ThreadRange(1, utils_cpu_count())

static void memory_allocation_slab_allocator_only_alloc(benchmark::State& state) {
    size_t object_size = state.range(0);
    uint32_t objects_count = state.range(1);

    thread_current_set_affinity(state.thread_index());

    std::vector<void*> memptrs = std::vector<void*>(objects_count);

    for (auto _ : state) {
        for(long int i = 0; i < objects_count; i++) {
            benchmark::DoNotOptimize((memptrs[i] = slab_allocator_mem_alloc(object_size)));
        }
    }

    for(long int i = 0; i < objects_count; i++) {
        slab_allocator_mem_free(memptrs[i]);
    }
}

static void memory_allocation_slab_allocator_alloc_and_free(benchmark::State& state) {
    size_t object_size = state.range(0);
    uint32_t objects_count = state.range(1);

    thread_current_set_affinity(state.thread_index());

    std::vector<void*> memptrs = std::vector<void*>(objects_count);

    for (auto _ : state) {
        for(long int i = 0; i < objects_count; i++) {
            benchmark::DoNotOptimize((memptrs[i] = slab_allocator_mem_alloc(object_size)));
        }

        for(long int i = 0; i < objects_count; i++) {
            slab_allocator_mem_free(memptrs[i]);
        }
    }
}

static void memory_allocation_slab_allocator_fragment_memory(benchmark::State& state) {
    size_t object_size = state.range(0);
    uint32_t objects_count = state.range(1);
    uint32_t objects_count_1_of_4 = objects_count >> 2;

    thread_current_set_affinity(state.thread_index());

    std::random_device rd;
    std::mt19937 g(rd());
    std::vector<void*> memptrs = std::vector<void*>(objects_count);

    // The code below willingly causes memory fragmentation
    for (auto _ : state) {
        for(long int i = 0; i < objects_count; i++) {
            benchmark::DoNotOptimize((memptrs[i] = slab_allocator_mem_alloc(object_size)));
        }

        state.PauseTiming();
        std::shuffle(memptrs.begin(), memptrs.end(), g);
        state.ResumeTiming();

        // Free and re-allocate the first quarter
        // 1E  2   3   4
        for(long int i = 0; i < objects_count_1_of_4 * 1; i++) {
            slab_allocator_mem_free(memptrs[i]);
        }
        // 1EF 2F  3F  4F
        for(long int i = 0; i < objects_count_1_of_4 * 1; i++) {
            benchmark::DoNotOptimize((memptrs[i] = slab_allocator_mem_alloc(object_size)));
        }

        // Free the second and third quarter
        // 1EF 2E  3E  4
        for(long int i = objects_count_1_of_4 * 1; i < objects_count_1_of_4 * 3; i++) {
            slab_allocator_mem_free(memptrs[i]);
        }

        // Reallocate the third quarter
        // 1EF 2E  3EF 4
        for(long int i = objects_count_1_of_4 * 2; i < objects_count_1_of_4 * 3; i++) {
            benchmark::DoNotOptimize((memptrs[i] = slab_allocator_mem_alloc(object_size)));
        }

        // Free the last quarter
        // 1EF 2E  3EF 4E
        for(long int i = objects_count_1_of_4 * 3; i < objects_count_1_of_4 * 4; i++) {
            slab_allocator_mem_free(memptrs[i]);
        }

        // Reallocate the second quarter
        // 1EF 2EF 3EF 4E
        for(long int i = objects_count_1_of_4 * 1; i < objects_count_1_of_4 * 2; i++) {
            benchmark::DoNotOptimize((memptrs[i] = slab_allocator_mem_alloc(object_size)));
        }

        // Reallocate the last quarter
        // 1EF 2EF 3EF 4EF
        for(long int i = objects_count_1_of_4 * 3; i < objects_count_1_of_4 * 4; i++) {
            benchmark::DoNotOptimize((memptrs[i] = slab_allocator_mem_alloc(object_size)));
        }

        // Free everything
        for(long int i = 0; i < objects_count; i++) {
            slab_allocator_mem_free(memptrs[i]);
        }

        // Re-allocate and free everything
        for(long int i = 0; i < objects_count; i++) {
            benchmark::DoNotOptimize((memptrs[i] = slab_allocator_mem_alloc(object_size)));
        }
        for(long int i = 0; i < objects_count; i++) {
            slab_allocator_mem_free(memptrs[i]);
        }
    }
}

static void memory_allocation_os_malloc_only_alloc(benchmark::State& state) {
    size_t object_size = state.range(0);
    uint32_t objects_count = state.range(1);

    thread_current_set_affinity(state.thread_index());

    void** memptrs = (void**)malloc(sizeof(void*) * objects_count);
    memset(memptrs, 0, sizeof(void*) * objects_count);

    for (auto _ : state) {
        for(long int i = 0; i < objects_count; i++) {
            benchmark::DoNotOptimize((memptrs[i] = malloc(object_size)));
        }
    }

    for(long int i = 0; i < objects_count; i++) {
        free(memptrs[i]);
    }
}

static void memory_allocation_os_malloc_alloc_and_free(benchmark::State& state) {
    size_t object_size = state.range(0);
    uint32_t objects_count = state.range(1);

    thread_current_set_affinity(state.thread_index());

    void** memptrs = (void**)malloc(sizeof(void*) * objects_count);
    memset(memptrs, 0, sizeof(void*) * objects_count);

    for (auto _ : state) {
        for(long int i = 0; i < objects_count; i++) {
            benchmark::DoNotOptimize((memptrs[i] = malloc(object_size)));
        }

        for(long int i = 0; i < objects_count; i++) {
            free(memptrs[i]);
        }
    }
}

static void memory_allocation_os_malloc_fragment_memory(benchmark::State& state) {
    size_t object_size = state.range(0);
    uint32_t objects_count = state.range(1);
    uint32_t objects_count_1_of_4 = objects_count >> 2;

    thread_current_set_affinity(state.thread_index());

    std::random_device rd;
    std::mt19937 g(rd());
    std::vector<void*> memptrs = std::vector<void*>(objects_count);

    // The code below willingly causes memory fragmentation
    for (auto _ : state) {
        for(long int i = 0; i < objects_count; i++) {
            benchmark::DoNotOptimize((memptrs[i] = malloc(object_size)));
        }

        state.PauseTiming();
        std::shuffle(memptrs.begin(), memptrs.end(), g);
        state.ResumeTiming();

        // Free and re-allocate the first quarter
        // 1E  2   3   4
        for(long int i = 0; i < objects_count_1_of_4 * 1; i++) {
            free(memptrs[i]);
        }
        // 1EF 2F  3F  4F
        for(long int i = 0; i < objects_count_1_of_4 * 1; i++) {
            benchmark::DoNotOptimize((memptrs[i] = malloc(object_size)));
        }

        // Free the second and third quarter
        // 1EF 2E  3E  4
        for(long int i = objects_count_1_of_4 * 1; i < objects_count_1_of_4 * 3; i++) {
            free(memptrs[i]);
        }

        // Reallocate the third quarter
        // 1EF 2E  3EF 4
        for(long int i = objects_count_1_of_4 * 2; i < objects_count_1_of_4 * 3; i++) {
            benchmark::DoNotOptimize((memptrs[i] = malloc(object_size)));
        }

        // Free the last quarter
        // 1EF 2E  3EF 4E
        for(long int i = objects_count_1_of_4 * 3; i < objects_count_1_of_4 * 4; i++) {
            free(memptrs[i]);
        }

        // Reallocate the second quarter
        // 1EF 2EF 3EF 4E
        for(long int i = objects_count_1_of_4 * 1; i < objects_count_1_of_4 * 2; i++) {
            benchmark::DoNotOptimize((memptrs[i] = malloc(object_size)));
        }

        // Reallocate the last quarter
        // 1EF 2EF 3EF 4EF
        for(long int i = objects_count_1_of_4 * 3; i < objects_count_1_of_4 * 4; i++) {
            benchmark::DoNotOptimize((memptrs[i] = malloc(object_size)));
        }

        // Free everything
        for(long int i = 0; i < objects_count; i++) {
            free(memptrs[i]);
        }

        // Re-allocate and free everything
        for(long int i = 0; i < objects_count; i++) {
            benchmark::DoNotOptimize((memptrs[i] = malloc(object_size)));
        }
        for(long int i = 0; i < objects_count; i++) {
            free(memptrs[i]);
        }
    }
}
//
//BENCHMARK(memory_allocation_slab_allocator_only_alloc)
//    ->SET_BENCH_ARGS()
//    ->SET_BENCH_THREADS();
//BENCHMARK(memory_allocation_slab_allocator_alloc_and_free)
//    ->SET_BENCH_ARGS()
//    ->SET_BENCH_THREADS();
//BENCHMARK(memory_allocation_slab_allocator_fragment_memory)
//    ->SET_BENCH_ARGS()
//    ->SET_BENCH_THREADS();

BENCHMARK(memory_allocation_os_malloc_only_alloc)
    ->SET_BENCH_ARGS()
    ->SET_BENCH_THREADS();
BENCHMARK(memory_allocation_os_malloc_alloc_and_free)
    ->SET_BENCH_ARGS()
    ->SET_BENCH_THREADS();
BENCHMARK(memory_allocation_os_malloc_fragment_memory)
    ->SET_BENCH_ARGS()
    ->SET_BENCH_THREADS();
