#include <stdint.h>
#include <benchmark/benchmark.h>

#include "hashtable/hashtable_support_hash_search.h"

void set_thread_affinity(int thread_index) {
#if !defined(__MINGW32__)
    int res;
    cpu_set_t cpuset;
    pthread_t thread;

    CPU_ZERO(&cpuset);
    CPU_SET(thread_index % 12, &cpuset);

    thread = pthread_self();
    res = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (res != 0) {
        perror("pthread_setaffinity_np");
    }
#endif
}

uint32_t* init_hashes() {
    uint32_t* __attribute__((aligned(16))) hashes =
            (uint32_t*)aligned_alloc(256, sizeof(uint32_t) * 16);

    for(uint32_t i = 0; i < 16; i++) {
        uint32_t v = i;
        hashes[i] = v;
    }

    return hashes;
}

#define BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH(METHOD) \
    extern int8_t hashtable_support_hash_search_##METHOD(uint32_t hash, volatile uint32_t* hashes); \
    void bench_template_hashtable_support_hash_search_##METHOD(benchmark::State& state) { \
        volatile int found_index; \
        static uint32_t* hashes; \
        \
        if (state.thread_index == 0) { hashes = init_hashes(); } \
        \
        set_thread_affinity(state.thread_index); \
        \
        for (auto _ : state) { \
            for(int i = 0; i < 1000000; i++) { \
                benchmark::DoNotOptimize( \
                        found_index = hashtable_support_hash_search_##METHOD(state.range(0), hashes) \
                ); \
            } \
        } \
        \
        if (state.thread_index == 0) { free(hashes); } \
    } \
    BENCHMARK(bench_template_hashtable_support_hash_search_##METHOD) \
        ->Iterations(100) \
        ->Arg(0) \
        ->Arg(2) \
        ->Arg(3) \
        ->Arg(7) \
        ->Arg(8) \
        ->Arg(12) \
        ->Arg(13) \
        ->Arg(15) \
        ->Threads(1) \
        ->Threads(2) \
        ->Threads(4) \
        ->Threads(8) \
        ->UseRealTime()


BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH(avx2);
BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH(loop);
