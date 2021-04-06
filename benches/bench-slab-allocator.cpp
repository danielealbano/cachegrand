#include <string.h>
#include <benchmark/benchmark.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "thread.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "slab_allocator.h"

#define SET_BENCH_ARGS() \
    Args({64, 1000})-> \
    Args({64, 10000})-> \
    Args({64, 100000})-> \
    Args({128, 1000})-> \
    Args({128, 10000})-> \
    Args({128, 100000})-> \
    Args({256, 1000})-> \
    Args({256, 10000})-> \
    Args({256, 100000})-> \
    Args({512, 1000})-> \
    Args({512, 10000})-> \
    Args({512, 100000})-> \
    Args({1024, 1000})-> \
    Args({1024, 10000})-> \
    Args({1024, 100000})-> \
    Args({2048, 1000})-> \
    Args({2048, 10000})-> \
    Args({2048, 100000})-> \
    Args({4096, 1000})-> \
    Args({4096, 10000})-> \
    Args({4096, 100000})-> \
    Args({8192, 1000})-> \
    Args({8192, 10000})-> \
    Args({8192, 100000})

#define SET_BENCH_THREADS() \
    Threads(1)-> \
    Threads(2)-> \
    Threads(4)-> \
    Threads(8)-> \
    Threads(6)-> \
    Threads(12)

static void memory_allocation_slab_allocator(benchmark::State& state) {
    size_t object_size = state.range(0);
    uint32_t objects_count = state.range(1);

    thread_current_set_affinity(state.thread_index);

    void** memptrs = (void**)malloc(sizeof(void*) * objects_count);

    for (auto _ : state) {
        for(long int i = 0; i < objects_count; i++) {
            benchmark::DoNotOptimize((memptrs[i] = slab_allocator_mem_alloc(object_size)));
        }
    }

    for(long int i = 0; i < objects_count; i++) {
        slab_allocator_mem_free(memptrs[i]);
    }
}

static void memory_allocation_os_malloc(benchmark::State& state) {
    size_t object_size = state.range(0);
    uint32_t objects_count = state.range(1);

    thread_current_set_affinity(state.thread_index);

    void** memptrs = (void**)malloc(sizeof(void*) * objects_count);

    for (auto _ : state) {
        for(long int i = 0; i < objects_count; i++) {
            benchmark::DoNotOptimize((memptrs[i] = malloc(object_size)));
        }
    }

    for(long int i = 0; i < objects_count; i++) {
        free(memptrs[i]);
    }
}

BENCHMARK(memory_allocation_slab_allocator)->SET_BENCH_ARGS()->SET_BENCH_THREADS()->Iterations(1)->Repetitions(10)->DisplayAggregatesOnly(true);
BENCHMARK(memory_allocation_os_malloc)->SET_BENCH_ARGS()->SET_BENCH_THREADS()->Iterations(1)->Repetitions(10)->DisplayAggregatesOnly(true);
