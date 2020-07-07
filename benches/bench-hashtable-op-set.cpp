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

#include "../tests/support.h"
#include "../tests/fixtures-hashtable.h"

#include "bench-support.h"

#define KEYSET_MAX_SIZE             (double)0x7FFFFFFFu * (double)0.75
#define KEYSET_GENERATOR_METHOD     TEST_SUPPORT_RANDOM_KEYS_GEN_FUNC_RANDOM_STR_RANDOM_LENGTH

#define SET_BENCH_ARGS_HT_SIZE_AND_KEYS() \
    Args({0x0000FFFFu, 75})-> \
    Args({0x000FFFFFu, 75})-> \
    Args({0x001FFFFFu, 75})-> \
    Args({0x007FFFFFu, 75})-> \
    Args({0x00FFFFFFu, 75})-> \
    Args({0x01FFFFFFu, 50})-> \
    Args({0x01FFFFFFu, 75})-> \
    Args({0x07FFFFFFu, 50})-> \
    Args({0x07FFFFFFu, 75})-> \
    Args({0x0FFFFFFFu, 50})-> \
    Args({0x0FFFFFFFu, 75})-> \
    Args({0x1FFFFFFFu, 50})-> \
    Args({0x1FFFFFFFu, 75})-> \
    Args({0x3FFFFFFFu, 50})-> \
    Args({0x3FFFFFFFu, 75})-> \
    Args({0x7FFFFFFFu, 50})-> \
    Args({0x7FFFFFFFu, 75})

#define SET_BENCH_THREADS \
    Threads(1)-> \
    Threads(2)-> \
    Threads(4)-> \
    Threads(8)-> \
    Threads(16)-> \
    Threads(32)-> \
    Threads(64)-> \
    Threads(128)-> \
    Threads(256)-> \
    Threads(512)-> \
    Threads(1024)-> \
    Threads(2048)

#define SET_BENCH_ITERATIONS \
    Iterations(1)->\
    Repetitions(25)->\
    DisplayAggregatesOnly(true)

#define CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS() \
    UseRealTime()-> \
    SET_BENCH_ARGS_HT_SIZE_AND_KEYS()-> \
    SET_BENCH_ITERATIONS-> \
    SET_BENCH_THREADS


static char* keyset = NULL;
static uint64_t keyset_size = 0;

static void hashtable_op_set_keyset_init_notatest(benchmark::State& state) {
    keyset_size = KEYSET_MAX_SIZE + 1;

    keyset = test_support_init_keys(
            keyset_size,
            KEYSET_GENERATOR_METHOD,
            544498304);

    state.SkipWithError("Not a test, skipping");
}

static void hashtable_op_set_keyset_cleanup_notatest(benchmark::State& state) {
    test_support_free_keys(keyset, keyset_size);

    keyset = NULL;
    keyset_size = 0;

    state.SkipWithError("Not a test, skipping");
}

static void hashtable_op_set_new(benchmark::State& state) {
    static hashtable_t* hashtable;
    static uint64_t requested_keyset_size;
    bool result;
    char error_message[150] = {0};

    if (bench_support_check_if_too_many_threads_per_core(state.threads, BENCHES_MAX_THREADS_PER_CORE)) {
        sprintf(error_message, "Too many threads per core, max allowed <%d>", BENCHES_MAX_THREADS_PER_CORE);
        state.SkipWithError(error_message);
        return;
    }

    if (state.thread_index == 0) {
        hashtable = test_support_init_hashtable(state.range(0));

        double requested_load_factor = (double)state.range(1) / 100;
        requested_keyset_size = (double)hashtable->ht_current->buckets_count * requested_load_factor;
        if (requested_keyset_size > keyset_size) {
            sprintf(
                    error_message,
                    "The requested keyset size of <%lu> is greater than the one available of <%lu>, can't continue",
                    requested_keyset_size,
                    keyset_size);
            state.SkipWithError(error_message);
            return;
        }

        // Flush the cpu DCACHE related to the portion of the keyset used by the bench before starting the test
        test_support_flush_data_cache(keyset, TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL * requested_keyset_size);
    }

    test_support_set_thread_affinity(state.thread_index);

    for (auto _ : state) {
        for(long int i = state.thread_index; i < requested_keyset_size; i += state.threads) {
            uint64_t keyset_offset = TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL * i;
            char* key = keyset + keyset_offset;

            benchmark::DoNotOptimize((result = hashtable_op_set(
                    hashtable,
                    key,
                    strlen(key),
                    i)));

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
    }
}

static void hashtable_op_set_update(benchmark::State& state) {
    static hashtable_t* hashtable;
    static uint64_t requested_keyset_size;
    bool result;
    char error_message[150] = {0};

    if (bench_support_check_if_too_many_threads_per_core(state.threads, BENCHES_MAX_THREADS_PER_CORE)) {
        sprintf(error_message, "Too many threads per core, max allowed <%d>", BENCHES_MAX_THREADS_PER_CORE);
        state.SkipWithError(error_message);
        return;
    }

    if (state.thread_index == 0) {
        hashtable = test_support_init_hashtable(state.range(0));

        double requested_load_factor = (double)state.range(1) / 100;
        requested_keyset_size = (double)hashtable->ht_current->buckets_count * requested_load_factor;
        if (requested_keyset_size > keyset_size) {
            sprintf(
                    error_message,
                    "The requested keyset size of <%lu> is greater than the one available of <%lu>, can't continue",
                    requested_keyset_size,
                    keyset_size);
            state.SkipWithError(error_message);
            return;
        }

        bool result = test_support_hashtable_prefill(hashtable, keyset, test_value_1, requested_keyset_size);

        if (!result) {
            hashtable_free(hashtable);

            sprintf(error_message, "Unable to prefill the hashtable with <%lu> keys", requested_keyset_size);
            state.SkipWithError(error_message);
            return;
        }

        // Flush the cpu DCACHE related to the portion of the keyset used by the bench before starting the test
        test_support_flush_data_cache(keyset, TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL * requested_keyset_size);
    }

    test_support_set_thread_affinity(state.thread_index);

    for (auto _ : state) {
        for(long int i = state.thread_index; i < requested_keyset_size; i += state.threads) {
            char* key = keyset + (TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL * i);

            benchmark::DoNotOptimize((result = hashtable_op_set(
                    hashtable,
                    key,
                    strlen(key),
                    i)));

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
    }
}

BENCHMARK(hashtable_op_set_keyset_init_notatest)->Iterations(1)->Threads(1)->Repetitions(1);

BENCHMARK(hashtable_op_set_new)
    ->CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS();
BENCHMARK(hashtable_op_set_update)
    ->CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS();

BENCHMARK(hashtable_op_set_keyset_cleanup_notatest)->Iterations(1)->Threads(1)->Repetitions(1);
