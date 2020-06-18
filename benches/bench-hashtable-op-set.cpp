#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <benchmark/benchmark.h>

#include "exttypes.h"
#include "spinlock.h"
#include "log.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_op_set.h"
#include "hashtable/hashtable_op_get.h"

#include "../tests/test-support.h"
#include "../tests/fixtures-hashtable.h"

#include "bench-support.h"

#define SET_BENCH_ARGS_HT_SIZE_AND_KEYS(keys_gen_func_name) \
    Args({0x0000FFFFu, 75, keys_gen_func_name})-> \
    Args({0x000FFFFFu, 75, keys_gen_func_name})-> \
    Args({0x001FFFFFu, 75, keys_gen_func_name})-> \
    Args({0x007FFFFFu, 75, keys_gen_func_name})-> \
    Args({0x00FFFFFFu, 75, keys_gen_func_name})-> \
    Args({0x01FFFFFFu, 50, keys_gen_func_name})-> \
    Args({0x01FFFFFFu, 75, keys_gen_func_name})-> \
    Args({0x07FFFFFFu, 50, keys_gen_func_name})-> \
    Args({0x07FFFFFFu, 75, keys_gen_func_name})-> \
    Args({0x0FFFFFFFu, 50, keys_gen_func_name})-> \
    Args({0x0FFFFFFFu, 75, keys_gen_func_name})-> \
    Args({0x1FFFFFFFu, 50, keys_gen_func_name})-> \
    Args({0x1FFFFFFFu, 75, keys_gen_func_name})-> \
    Args({0x3FFFFFFFu, 50, keys_gen_func_name})-> \
    Args({0x3FFFFFFFu, 75, keys_gen_func_name})-> \
    Args({0x7FFFFFFFu, 50, keys_gen_func_name})-> \
    Args({0x7FFFFFFFu, 75, keys_gen_func_name})

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

#define SET_BENCH_ITERATIONS \
    Iterations(1)->\
    Repetitions(10)->\
    DisplayAggregatesOnly(true)

#define CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS(keys_gen_func_name) \
    UseRealTime()-> \
    SET_BENCH_ARGS_HT_SIZE_AND_KEYS(keys_gen_func_name)-> \
    SET_BENCH_ITERATIONS-> \
    SET_BENCH_THREADS

static void hashtable_op_set_new(benchmark::State& state) {
    static hashtable_t* hashtable;
    static char* keys;
    double requested_load_factor;
    static uint64_t keys_count = 0;
    char error_message[150] = {0};
    static uint8_t keys_generator_method = UINT8_MAX;
    uint64_t hashtable_initial_size = state.range(0);
    uint8_t requested_load_factor_perc = state.range(1);

    if (bench_support_check_if_too_many_threads_per_core(state.threads, BENCHES_MAX_THREADS_PER_CORE)) {
        sprintf(error_message, "Too many threads per core, max allowed <%d>", BENCHES_MAX_THREADS_PER_CORE);
        state.SkipWithError(error_message);
        return;
    }

    if (state.thread_index == 0) {
        hashtable = test_support_init_hashtable(hashtable_initial_size);
        requested_load_factor = (double)requested_load_factor_perc / 100;
        keys_count = (double)hashtable->ht_current->buckets_count * requested_load_factor;
        keys_generator_method = state.range(2);

        LOG_DI("Generating <%lu> keys for a load factor of <%.02f> with generator <%d>",
                keys_count,
                requested_load_factor,
                keys_generator_method);

        keys = test_support_init_keys(keys_count, keys_generator_method);
    }

    test_support_set_thread_affinity(state.thread_index);

    for (auto _ : state) {
        for(long int i = state.thread_index; i < keys_count; i += state.threads) {
            uint64_t keys_offset = TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL * i;
            char* key = keys + keys_offset;


            bool result = hashtable_op_set(
                    hashtable,
                    key,
                    strlen(key),
                    i);

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
        for(long int i = 0; i < keys_count; i++) {
            uint64_t value;
            char* key = keys + (TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL * i);

            bool result = hashtable_op_get(
                    hashtable,
                    key,
                    strlen(key),
                    (hashtable_value_data_t*)&value);

            if (!result) {
                LOG_DI("Can't find key <%s>", key);
            } else if (value != i) {
                LOG_DI("For the key <%s> there is a value mismatch, found <%lu> expected <%lu>", key, value, i);
            }
        }

        bench_support_collect_hashtable_stats_and_update_state(state, hashtable);
        hashtable_free(hashtable);
        test_support_free_keys(keys, keys_count);
    }
}

static void hashtable_op_set_update(benchmark::State& state) {
    static hashtable_t* hashtable;
    static char* keys;
    double requested_load_factor;
    static uint64_t keys_count = 0;
    char error_message[150] = {0};
    static uint8_t keys_generator_method = UINT8_MAX;
    uint64_t hashtable_initial_size = state.range(0);
    uint8_t requested_load_factor_perc = state.range(1);

    if (bench_support_check_if_too_many_threads_per_core(state.threads, BENCHES_MAX_THREADS_PER_CORE)) {
        sprintf(error_message, "Too many threads per core, max allowed <%d>", BENCHES_MAX_THREADS_PER_CORE);
        state.SkipWithError(error_message);
        return;
    }

    if (state.thread_index == 0) {
        hashtable = test_support_init_hashtable(hashtable_initial_size);
        requested_load_factor = (double)requested_load_factor_perc / 100;
        keys_count = (double)hashtable->ht_current->buckets_count * requested_load_factor;
        keys_generator_method = state.range(2);

        LOG_DI("Generating <%lu> keys for a load factor of <%.02f> with generator <%d>",
               keys_count,
               requested_load_factor,
               keys_generator_method);

        keys = test_support_init_keys(keys_count, keys_generator_method);
        bool result = test_support_hashtable_prefill(hashtable, keys, test_value_1, state.range(1));

        if (!result) {
            hashtable_free(hashtable);
            test_support_free_keys(keys, keys_count);

            sprintf(error_message, "Unable to pre-fill the hashtable with <%lu> keys", keys_count);
            state.SkipWithError(error_message);
            return;
        }
    }

    test_support_set_thread_affinity(state.thread_index);

    for (auto _ : state) {
        for(long int i = state.thread_index; i < state.range(1); i += state.threads) {
            char* key = keys + (TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL * i);

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
        for(long int i = 0; i < keys_count; i++) {
            uint64_t value;
            char* key = keys + (TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL * i);

            bool result = hashtable_op_get(
                    hashtable,
                    key,
                    strlen(key),
                    (hashtable_value_data_t*)&value);

            if (!result) {
                LOG_DI("Can't find key <%s>", key);
            } else if (value != i) {
                LOG_DI("For the key <%s> there is a value mismatch, found <%lu> expected <%lu>", key, value, i);
            }
        }

        bench_support_collect_hashtable_stats_and_update_state(state, hashtable);
        hashtable_free(hashtable);
        test_support_free_keys(keys, keys_count);
    }
}

BENCHMARK(hashtable_op_set_new)
    ->CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS(RANDOM_KEYS_GEN_FUNC_RANDOM_LENGTH);
BENCHMARK(hashtable_op_set_update)
    ->CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS(RANDOM_KEYS_GEN_FUNC_RANDOM_LENGTH);

