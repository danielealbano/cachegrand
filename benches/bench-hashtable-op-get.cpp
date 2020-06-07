#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <benchmark/benchmark.h>

#include "xalloc.h"
#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_support_index.h"
#include "hashtable/hashtable_op_get.h"

#include "../tests/fixtures-hashtable.h"

#define HASHTABLE_OP_GET_BENCHS_ARGS \
   Arg(1522U)->Arg(135798U)->Arg(1031398U)->Arg(17622551U)->Arg(89214403U)->Arg(133821599U) \
   ->Iterations(10000000);

static void hashtable_op_get_notfound(benchmark::State& state) {
    hashtable_config_t* hashtable_config;
    hashtable_t* hashtable;
    hashtable_value_data_t value;

    hashtable_config = hashtable_config_init();
    hashtable_config->initial_size = state.range(0);
    hashtable_config->can_auto_resize = false;

    hashtable = hashtable_init(hashtable_config);

    for (auto _ : state) {
        hashtable_op_get(
                hashtable,
                test_key_1,
                test_key_1_len,
                &value);
    }

    hashtable_free(hashtable);
}

static void hashtable_op_get_found_key_inline(benchmark::State& state) {
    hashtable_config_t* hashtable_config;
    hashtable_t* hashtable;
    hashtable_value_data_t value;
    bool result;
    char error_message[150] = {0};

    hashtable_config = hashtable_config_init();
    hashtable_config->initial_size = state.range(0);
    hashtable_config->can_auto_resize = false;

    hashtable = hashtable_init(hashtable_config);

    HASHTABLE_BUCKET_NEW_KEY_INLINE(
            test_key_1_hash % hashtable->ht_current->buckets_count,
            test_key_1_hash,
            test_key_1,
            test_key_1_len,
            test_value_1);

    for (auto _ : state) {
        benchmark::DoNotOptimize((result = hashtable_op_get(
                hashtable,
                test_key_1,
                test_key_1_len,
                &value)));

        if (!result) {
            sprintf(
                    error_message,
                    "Unable to get the key <%s> with index <%ld> for the thread <%d>",
                    test_key_1,
                    test_key_1_hash % hashtable->ht_current->buckets_size,
                    state.thread_index);
            state.SkipWithError(error_message);
            break;
        }
    }

    hashtable_free(hashtable);
}


static void hashtable_op_get_found_key_external(benchmark::State& state) {
    hashtable_config_t* hashtable_config;
    hashtable_t* hashtable;
    hashtable_value_data_t value;
    bool result;
    char error_message[150] = {0};

    hashtable_config = hashtable_config_init();
    hashtable_config->initial_size = state.range(0);
    hashtable_config->can_auto_resize = false;

    hashtable = hashtable_init(hashtable_config);

    HASHTABLE_BUCKET_NEW_KEY_EXTERNAL(
            test_key_1_hash % hashtable->ht_current->buckets_count,
            test_key_1_hash,
            test_key_1,
            test_key_1_len,
            test_value_1);

    for (auto _ : state) {
        benchmark::DoNotOptimize((result = hashtable_op_get(
                hashtable,
                test_key_1,
                test_key_1_len,
                &value)));

        if (!result) {
            sprintf(
                    error_message,
                    "Unable to get the key <%s> with index <%ld> for the thread <%d>",
                    test_key_1,
                    test_key_1_hash % hashtable->ht_current->buckets_size,
                    state.thread_index);
            state.SkipWithError(error_message);
            break;
        }
    }

    hashtable_free(hashtable);
}

BENCHMARK(hashtable_op_get_notfound)->HASHTABLE_OP_GET_BENCHS_ARGS;
BENCHMARK(hashtable_op_get_found_key_inline)->HASHTABLE_OP_GET_BENCHS_ARGS;
BENCHMARK(hashtable_op_get_found_key_external)->HASHTABLE_OP_GET_BENCHS_ARGS;
