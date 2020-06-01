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

#define BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH_FUNC(METHOD, SUFFIX) \
    void bench_template_hashtable_support_hash_search_##METHOD##_##SUFFIX(benchmark::State& state) { \
        volatile int found_index; \
        static uint32_t* hashes; \
        \
        if (state.thread_index == 0) { hashes = init_hashes(); } \
        \
        for (auto _ : state) { \
            benchmark::DoNotOptimize( \
                    found_index = hashtable_support_hash_search_##METHOD(state.range(0), hashes) \
            ); \
        } \
        \
        if (state.thread_index == 0) { free(hashes); } \
    }

#define BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH_ST(METHOD) \
    BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH_FUNC(METHOD, st) \
    BENCHMARK(bench_template_hashtable_support_hash_search_##METHOD##_st) \
        ->Iterations(100000000) \
        ->Arg(0)->Arg(1)->Arg(2)->Arg(3)->Arg(4)->Arg(5)->Arg(6)->Arg(7) \
        ->Arg(8)->Arg(9)->Arg(10)->Arg(11)->Arg(12)->Arg(13)->Arg(14)->Arg(15)

#define BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH_MT(METHOD) \
    BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH_FUNC(METHOD, mt) \
    BENCHMARK(bench_template_hashtable_support_hash_search_##METHOD##_mt) \
        ->Iterations(100000000) \
        ->Arg(0)->Arg(1)->Arg(2)->Arg(3)->Arg(4)->Arg(5)->Arg(6)->Arg(7) \
        ->Arg(8)->Arg(9)->Arg(10)->Arg(11)->Arg(12)->Arg(13)->Arg(14)->Arg(15) \
        ->Threads(2)->Threads(4)->Threads(8)->Threads(16)->Threads(32)

BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH_ST(avx2);
BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH_ST(loop);
BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH_MT(avx2);
BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH_MT(loop);

