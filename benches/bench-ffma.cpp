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

#include "mimalloc.h"

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "fatal.h"
#include "spinlock.h"
#include "thread.h"
#include "hugepages.h"
#include "utils_cpu.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma_page_cache.h"
#include "memory_allocator/ffma.h"
#include "memory_allocator/ffma_thread_cache.h"
#include "xalloc.h"

#include "benchmark-program-simple.hpp"

// This test requires about 67gb (33856) of hugepages to test up to 64 threads, to reduce the amount of memory required
// reduce the defined value below and also the TEST_ALLOCATIONS_COUNT_PER_THREAD, currently set to 16 * 1024
// allocations.
// If a machine with a lot of threads is used it's strongly suggested to reduce the TEST_ALLOCATIONS_COUNT_PER_THREAD to
// 4096 or 8192 as that will be the amount of allocations carried out per thread.
#define TEST_WARMPUP_HUGEPAGES_CACHE_COUNT 20000
#define TEST_ALLOCATIONS_COUNT_PER_THREAD (16 * 1024)

// It is possible to control the amount of threads used for the test tuning the two defines below
#define TEST_THREADS_RANGE_BEGIN (1)
#define TEST_THREADS_RANGE_END (utils_cpu_count())

static void memory_allocation_ffma_hugepages_warmup_cache(benchmark::State& state) {
    void **hugepages = (void**) malloc(sizeof(void*) * TEST_WARMPUP_HUGEPAGES_CACHE_COUNT);
    size_t os_page_size = xalloc_get_page_size();
    for(int hugepage_index = 0; hugepage_index < TEST_WARMPUP_HUGEPAGES_CACHE_COUNT; hugepage_index++) {
        char *addr;
        char *start_addr = (char*) ffma_page_cache_pop();

        if (start_addr == nullptr) {
            FATAL(
                    "bench-slab-allocator",
                    "Not enough hugepages, needed %d but available %d",
                    TEST_WARMPUP_HUGEPAGES_CACHE_COUNT,
                    hugepage_index);
        }

        hugepages[hugepage_index] = start_addr;
    }

    for(int hugepage_index = 0; hugepage_index < TEST_WARMPUP_HUGEPAGES_CACHE_COUNT; hugepage_index++) {
        ffma_page_cache_push(hugepages[hugepage_index]);
    }

    state.SkipWithError("Not a test");
}

static void memory_allocation_ffma_only_alloc(benchmark::State& state) {
    size_t object_size = state.range(0);
    uint32_t objects_count = state.range(1);

    thread_current_set_affinity(state.thread_index());

    std::vector<void*> memptrs = std::vector<void*>(objects_count);

    for (auto _ : state) {
        for(long int i = 0; i < objects_count; i++) {
#if DEBUG == 1
            memptrs[i] = ffma_mem_alloc(object_size);
#else
            benchmark::DoNotOptimize((memptrs[i] = ffma_mem_alloc(object_size)));
#endif
        }
    }

    for(long int i = 0; i < objects_count; i++) {
        ffma_mem_free(memptrs[i]);
    }
}

static void memory_allocation_ffma_alloc_and_free(benchmark::State& state) {
    size_t object_size = state.range(0);
    uint32_t objects_count = state.range(1);

    thread_current_set_affinity(state.thread_index());

    std::vector<void*> memptrs = std::vector<void*>(objects_count);

    for (auto _ : state) {
        for(long int i = 0; i < objects_count; i++) {
#if DEBUG == 1
            memptrs[i] = ffma_mem_alloc(object_size);
#else
            benchmark::DoNotOptimize((memptrs[i] = ffma_mem_alloc(object_size)));
#endif
        }

        for(long int i = 0; i < objects_count; i++) {
            ffma_mem_free(memptrs[i]);
        }
    }
}

static void memory_allocation_ffma_fragment_memory(benchmark::State& state) {
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
#if DEBUG == 1
            memptrs[i] = ffma_mem_alloc(object_size);
#else
            benchmark::DoNotOptimize((memptrs[i] = ffma_mem_alloc(object_size)));
#endif
        }

        state.PauseTiming();
        std::shuffle(memptrs.begin(), memptrs.end(), g);
        state.ResumeTiming();

        // Free and re-allocate the first quarter
        // 1E  2   3   4
        for(long int i = 0; i < objects_count_1_of_4 * 1; i++) {
            ffma_mem_free(memptrs[i]);
        }
        // 1EF 2F  3F  4F
        for(long int i = 0; i < objects_count_1_of_4 * 1; i++) {
#if DEBUG == 1
            memptrs[i] = ffma_mem_alloc(object_size);
#else
            benchmark::DoNotOptimize((memptrs[i] = ffma_mem_alloc(object_size)));
#endif
        }

        // Free the second and third quarter
        // 1EF 2E  3E  4
        for(long int i = objects_count_1_of_4 * 1; i < objects_count_1_of_4 * 3; i++) {
            ffma_mem_free(memptrs[i]);
        }

        // Reallocate the third quarter
        // 1EF 2E  3EF 4
        for(long int i = objects_count_1_of_4 * 2; i < objects_count_1_of_4 * 3; i++) {
#if DEBUG == 1
            memptrs[i] = ffma_mem_alloc(object_size);
#else
            benchmark::DoNotOptimize((memptrs[i] = ffma_mem_alloc(object_size)));
#endif
        }

        // Free the last quarter
        // 1EF 2E  3EF 4E
        for(long int i = objects_count_1_of_4 * 3; i < objects_count_1_of_4 * 4; i++) {
            ffma_mem_free(memptrs[i]);
        }

        // Reallocate the second quarter
        // 1EF 2EF 3EF 4E
        for(long int i = objects_count_1_of_4 * 1; i < objects_count_1_of_4 * 2; i++) {
#if DEBUG == 1
            memptrs[i] = ffma_mem_alloc(object_size);
#else
            benchmark::DoNotOptimize((memptrs[i] = ffma_mem_alloc(object_size)));
#endif
        }

        // Reallocate the last quarter
        // 1EF 2EF 3EF 4EF
        for(long int i = objects_count_1_of_4 * 3; i < objects_count_1_of_4 * 4; i++) {
#if DEBUG == 1
            memptrs[i] = ffma_mem_alloc(object_size);
#else
            benchmark::DoNotOptimize((memptrs[i] = ffma_mem_alloc(object_size)));
#endif
        }

        // Free everything
        for(long int i = 0; i < objects_count; i++) {
            ffma_mem_free(memptrs[i]);
        }

        // Re-allocate and free everything
        for(long int i = 0; i < objects_count; i++) {
#if DEBUG == 1
            memptrs[i] = ffma_mem_alloc(object_size);
#else
            benchmark::DoNotOptimize((memptrs[i] = ffma_mem_alloc(object_size)));
#endif
        }
        for(long int i = 0; i < objects_count; i++) {
            ffma_mem_free(memptrs[i]);
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

static void memory_allocation_mimalloc_only_alloc(benchmark::State& state) {
    size_t object_size = state.range(0);
    uint32_t objects_count = state.range(1);

    thread_current_set_affinity(state.thread_index());

    void** memptrs = (void**)mi_zalloc(sizeof(void*) * objects_count);

    for (auto _ : state) {
        for(long int i = 0; i < objects_count; i++) {
            benchmark::DoNotOptimize((memptrs[i] = mi_malloc(object_size)));
        }
    }

    for(long int i = 0; i < objects_count; i++) {
        mi_free(memptrs[i]);
    }
}

static void memory_allocation_mimalloc_alloc_and_free(benchmark::State& state) {
    size_t object_size = state.range(0);
    uint32_t objects_count = state.range(1);

    thread_current_set_affinity(state.thread_index());

    void** memptrs = (void**)mi_zalloc(sizeof(void*) * objects_count);

    for (auto _ : state) {
        for(long int i = 0; i < objects_count; i++) {
            benchmark::DoNotOptimize((memptrs[i] = mi_malloc(object_size)));
        }

        for(long int i = 0; i < objects_count; i++) {
            mi_free(memptrs[i]);
        }
    }
}

static void memory_allocation_mimalloc_fragment_memory(benchmark::State& state) {
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
            benchmark::DoNotOptimize((memptrs[i] = mi_malloc(object_size)));
        }

        state.PauseTiming();
        std::shuffle(memptrs.begin(), memptrs.end(), g);
        state.ResumeTiming();

        // Free and re-allocate the first quarter
        // 1E  2   3   4
        for(long int i = 0; i < objects_count_1_of_4 * 1; i++) {
            mi_free(memptrs[i]);
        }
        // 1EF 2F  3F  4F
        for(long int i = 0; i < objects_count_1_of_4 * 1; i++) {
            benchmark::DoNotOptimize((memptrs[i] = mi_malloc(object_size)));
        }

        // Free the second and third quarter
        // 1EF 2E  3E  4
        for(long int i = objects_count_1_of_4 * 1; i < objects_count_1_of_4 * 3; i++) {
            mi_free(memptrs[i]);
        }

        // Reallocate the third quarter
        // 1EF 2E  3EF 4
        for(long int i = objects_count_1_of_4 * 2; i < objects_count_1_of_4 * 3; i++) {
            benchmark::DoNotOptimize((memptrs[i] = mi_malloc(object_size)));
        }

        // Free the last quarter
        // 1EF 2E  3EF 4E
        for(long int i = objects_count_1_of_4 * 3; i < objects_count_1_of_4 * 4; i++) {
            mi_free(memptrs[i]);
        }

        // Reallocate the second quarter
        // 1EF 2EF 3EF 4E
        for(long int i = objects_count_1_of_4 * 1; i < objects_count_1_of_4 * 2; i++) {
            benchmark::DoNotOptimize((memptrs[i] = mi_malloc(object_size)));
        }

        // Reallocate the last quarter
        // 1EF 2EF 3EF 4EF
        for(long int i = objects_count_1_of_4 * 3; i < objects_count_1_of_4 * 4; i++) {
            benchmark::DoNotOptimize((memptrs[i] = mi_malloc(object_size)));
        }

        // Free everything
        for(long int i = 0; i < objects_count; i++) {
            mi_free(memptrs[i]);
        }

        // Re-allocate and free everything
        for(long int i = 0; i < objects_count; i++) {
            benchmark::DoNotOptimize((memptrs[i] = mi_malloc(object_size)));
        }
        for(long int i = 0; i < objects_count; i++) {
            mi_free(memptrs[i]);
        }
    }
}

static void BenchArguments(benchmark::internal::Benchmark* b) {
    b
            ->ArgsProduct({
                                  { FFMA_OBJECT_SIZE_16, FFMA_OBJECT_SIZE_32, FFMA_OBJECT_SIZE_64, FFMA_OBJECT_SIZE_128, FFMA_OBJECT_SIZE_256,
                                          FFMA_OBJECT_SIZE_512, FFMA_OBJECT_SIZE_1024, FFMA_OBJECT_SIZE_2048, FFMA_OBJECT_SIZE_4096,
                                          FFMA_OBJECT_SIZE_8192, FFMA_OBJECT_SIZE_16384, FFMA_OBJECT_SIZE_32768, FFMA_OBJECT_SIZE_65536 },
                                  { TEST_ALLOCATIONS_COUNT_PER_THREAD }
                          })
            ->ThreadRange(TEST_THREADS_RANGE_BEGIN, TEST_THREADS_RANGE_END)
            ->Iterations(1)
            ->Repetitions(25)
            ->DisplayAggregatesOnly(true);
}

// Warmup the hugepages cache, has to be done only once, forces iterations and repetitions to 1 to do not waste time
BENCHMARK(memory_allocation_ffma_hugepages_warmup_cache)
        ->Iterations(1)
        ->Repetitions(1);

BENCHMARK(memory_allocation_ffma_only_alloc)
        ->Apply(BenchArguments);
BENCHMARK(memory_allocation_ffma_alloc_and_free)
        ->Apply(BenchArguments);
BENCHMARK(memory_allocation_ffma_fragment_memory)
        ->Apply(BenchArguments);

BENCHMARK(memory_allocation_os_malloc_only_alloc)
        ->Apply(BenchArguments);
BENCHMARK(memory_allocation_os_malloc_alloc_and_free)
        ->Apply(BenchArguments);
BENCHMARK(memory_allocation_os_malloc_fragment_memory)
        ->Apply(BenchArguments);

BENCHMARK(memory_allocation_mimalloc_only_alloc)
        ->Apply(BenchArguments);
BENCHMARK(memory_allocation_mimalloc_alloc_and_free)
        ->Apply(BenchArguments);
BENCHMARK(memory_allocation_mimalloc_fragment_memory)
        ->Apply(BenchArguments);
