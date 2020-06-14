#include <stdio.h>
#include <string.h>

#include <benchmark/benchmark.h>

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_op_set.h"

#include "../tests/test-support.h"
#include "../tests/fixtures-hashtable.h"

#include "bench-support.h"


#define SET_BENCH_ARGS_HT_SIZE_AND_KEYS(keys_gen_func_name) \
    Args({1522, (uint64_t)(1522.0 * 0.25), keys_gen_func_name})-> \
    Args({1522, (uint64_t)(1522.0 * 0.33), keys_gen_func_name})-> \
    Args({1522, (uint64_t)(1522.0 * 0.50), keys_gen_func_name})-> \
    Args({1522, (uint64_t)(1522.0 * 0.75), keys_gen_func_name})-> \
    Args({1522, (uint64_t)(1522.0 * 0.90), keys_gen_func_name})-> \
    Args({135798, (uint64_t)(135798.0 * 0.25), keys_gen_func_name})-> \
    Args({135798, (uint64_t)(135798.0 * 0.33), keys_gen_func_name})-> \
    Args({135798, (uint64_t)(135798.0 * 0.50), keys_gen_func_name})-> \
    Args({135798, (uint64_t)(135798.0 * 0.75), keys_gen_func_name})-> \
    Args({135798, (uint64_t)(135798.0 * 0.90), keys_gen_func_name})-> \
    Args({1031398, (uint64_t)(1031398.0 * 0.25), keys_gen_func_name})-> \
    Args({1031398, (uint64_t)(1031398.0 * 0.33), keys_gen_func_name})-> \
    Args({1031398, (uint64_t)(1031398.0 * 0.50), keys_gen_func_name})-> \
    Args({1031398, (uint64_t)(1031398.0 * 0.75), keys_gen_func_name})-> \
    Args({1031398, (uint64_t)(1031398.0 * 0.90), keys_gen_func_name})-> \
    Args({11748391, (uint64_t)(11748391.0 * 0.25), keys_gen_func_name})-> \
    Args({11748391, (uint64_t)(11748391.0 * 0.33), keys_gen_func_name})-> \
    Args({11748391, (uint64_t)(11748391.0 * 0.50), keys_gen_func_name})-> \
    Args({11748391, (uint64_t)(11748391.0 * 0.75), keys_gen_func_name})-> \
    Args({11748391, (uint64_t)(11748391.0 * 0.90), keys_gen_func_name})-> \
    Args({133821673, (uint64_t)(133821673.0 * 0.25), keys_gen_func_name})-> \
    Args({133821673, (uint64_t)(133821673.0 * 0.33), keys_gen_func_name})-> \
    Args({133821673, (uint64_t)(133821673.0 * 0.50), keys_gen_func_name})-> \
    Args({133821673, (uint64_t)(133821673.0 * 0.75), keys_gen_func_name})-> \
    Args({133821673, (uint64_t)(133821673.0 * 0.90), keys_gen_func_name})

#define SET_BENCH_ITERATIONS \
    Iterations(1)->Repetitions(10)->DisplayAggregatesOnly(true)

#define SET_BENCH_THREADS \
    Threads(1)-> \
    Threads(2)-> \
    Threads(4)-> \
    Threads(8)-> \
    Threads(16)-> \
    Threads(32)-> \
    Threads(64)-> \
    Threads(128)-> \
    Threads(256)

#define CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS(keys_gen_func_name) \
    UseRealTime()-> \
    SET_BENCH_ARGS_HT_SIZE_AND_KEYS(keys_gen_func_name)-> \
    SET_BENCH_ITERATIONS-> \
    SET_BENCH_THREADS

static void hashtable_op_set_new(benchmark::State& state) {
    static hashtable_t* hashtable;
    static char* keys;
    char error_message[150] = {0};

    if (bench_support_check_if_too_many_threads_per_core(state.threads, BENCHES_MAX_THREADS_PER_CORE)) {
        sprintf(error_message, "Too many threads per core, max allowed <%d>", BENCHES_MAX_THREADS_PER_CORE);
        state.SkipWithError(error_message);
        return;
    }

    if (state.thread_index == 0) {
        keys = test_support_init_keys(state.range(0), state.range(2));
        hashtable = test_support_init_hashtable(state.range(0));
    }

    test_support_set_thread_affinity(state.thread_index);

    for (auto _ : state) {
        for(long int i = state.thread_index; i < state.range(1); i += state.threads) {
            char* key = keys + (TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH * i);
            bool result = hashtable_op_set(
                    hashtable,
                    key,
                    strlen(key),
                    test_value_1);

            if (!result) {
                sprintf(
                        error_message,
                        "Unable to set the key <%s> with index <%ld> for the thread <%d>",
                        key,
                        i,
                        state.thread_index);
                state.SkipWithError(error_message);
                break;
            }
        }
    }

    if (state.thread_index == 0) {
        bench_support_collect_hashtable_stats_and_update_state(state, hashtable);
        hashtable_free(hashtable);
        test_support_free_keys(keys, state.range(0));
    }
}

static void hashtable_op_set_update(benchmark::State& state) {
    static hashtable_t* hashtable;
    static char* keys;
    char error_message[150] = {0};

    if (bench_support_check_if_too_many_threads_per_core(state.threads, BENCHES_MAX_THREADS_PER_CORE)) {
        sprintf(error_message, "Too many threads per core, max allowed <%d>", BENCHES_MAX_THREADS_PER_CORE);
        state.SkipWithError(error_message);
        return;
    }

    if (state.thread_index == 0) {
        keys = test_support_init_keys(state.range(0), state.range(2));
        hashtable = test_support_init_hashtable(state.range(0));
        bool result = test_support_hashtable_prefill(hashtable, keys, test_value_1, state.range(1));

        if (!result) {
            hashtable_free(hashtable);
            test_support_free_keys(keys, state.range(0));

            sprintf(error_message, "Unable to prefill the hashtable with <%lu> keys", state.range(1));
            state.SkipWithError(error_message);
            return;
        }
    }

    test_support_set_thread_affinity(state.thread_index);

    for (auto _ : state) {
        for(long int i = state.thread_index; i < state.range(1); i += state.threads) {
            char* key = keys + (TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH * i);

            bool result = hashtable_op_set(
                    hashtable,
                    key,
                    strlen(key),
                    test_value_1);

            if (!result) {
                sprintf(
                        error_message,
                        "Unable to set the key <%s> with index <%ld> for the thread <%d>",
                        key,
                        i,
                        state.thread_index);
                state.SkipWithError(error_message);
                break;
            }
        }
    }

    if (state.thread_index == 0) {
        bench_support_collect_hashtable_stats_and_update_state(state, hashtable);
        hashtable_free(hashtable);
        test_support_free_keys(keys, state.range(0));
    }
}

BENCHMARK(hashtable_op_set_new)
    ->CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS(RANDOM_KEYS_GEN_FUNC_RANDOM_LENGTH);
BENCHMARK(hashtable_op_set_update)
    ->CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS(RANDOM_KEYS_GEN_FUNC_RANDOM_LENGTH);

