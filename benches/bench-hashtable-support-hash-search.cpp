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
            (uint32_t*)aligned_alloc(16, sizeof(uint32_t) * 16);

    for(uint32_t i = 0; i < 16; i++) {
        hashes[i] = i;
    }
    hashes[14] = 0;
    hashes[15] = 0;

    return hashes;
}

#define BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH_FUNC_WRAPPER(METHOD, SUFFIX, ...) \
    void bench_template_hashtable_support_hash_search_##METHOD##_##SUFFIX(benchmark::State& state) { \
        uint32_t* hashes = NULL; \
        hashtable_support_hash_search_fp_t hashtable_support_hash_search_method = hashtable_support_hash_search_##METHOD; \
        \
        set_thread_affinity(state.thread_index); \
        \
        for (auto _ : state) { \
            hashes = init_hashes(); \
            __VA_ARGS__ \
            free(hashes); \
        } \
    } \
    BENCHMARK(bench_template_hashtable_support_hash_search_##METHOD##_##SUFFIX) \
        ->Iterations(10000000) \
        ->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(16)->Threads(32)->Threads(64);

#define BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH_FULL(METHOD) \
    BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH_FUNC_WRAPPER(METHOD, full, { \
        benchmark::DoNotOptimize(hashtable_support_hash_search_method(8, hashes)); \
        benchmark::DoNotOptimize(hashtable_support_hash_search_method(1, hashes)); \
        benchmark::DoNotOptimize(hashtable_support_hash_search_method(13, hashes)); \
        benchmark::DoNotOptimize(hashtable_support_hash_search_method(4, hashes)); \
        benchmark::DoNotOptimize(hashtable_support_hash_search_method(9, hashes)); \
        benchmark::DoNotOptimize(hashtable_support_hash_search_method(0, hashes)); \
        benchmark::DoNotOptimize(hashtable_support_hash_search_method(5, hashes)); \
        benchmark::DoNotOptimize(hashtable_support_hash_search_method(11, hashes)); \
        free(hashes); hashes = init_hashes(); \
        benchmark::DoNotOptimize(hashtable_support_hash_search_method(3, hashes)); \
        benchmark::DoNotOptimize(hashtable_support_hash_search_method(12, hashes)); \
        benchmark::DoNotOptimize(hashtable_support_hash_search_method(7, hashes)); \
        benchmark::DoNotOptimize(hashtable_support_hash_search_method(2, hashes)); \
        benchmark::DoNotOptimize(hashtable_support_hash_search_method(15, hashes)); \
        benchmark::DoNotOptimize(hashtable_support_hash_search_method(14, hashes)); \
        benchmark::DoNotOptimize(hashtable_support_hash_search_method(6, hashes)); \
        benchmark::DoNotOptimize(hashtable_support_hash_search_method(10, hashes)); \
    })

#define BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH_ALL(METHOD) \
    BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH_FULL(METHOD);

BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH_ALL(loop);
BENCH_TEMPLATE_HASHTABLE_SUPPORT_HASH_SEARCH_ALL(avx2);

BENCHMARK_MAIN();
