#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <benchmark/benchmark.h>

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_support_index.h"
#include "hashtable/hashtable_op_get.h"

#include "../tests/fixtures-hashtable.h"

#define HASHTABLE_OP_GET_BENCHS_ARGS \
   Arg(1522)->Arg(135798)->Arg(1031398)

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

    hashtable_config = hashtable_config_init();
    hashtable_config->initial_size = state.range(0);
    hashtable_config->can_auto_resize = false;

    hashtable = hashtable_init(hashtable_config);

    HASHTABLE_BUCKET_HASH_KEY_VALUE_SET_KEY_INLINE(
            test_index_1_buckets_count_53,
            test_key_1_hash,
            test_key_1,
            test_key_1_len,
            test_value_1)

    for (auto _ : state) {
        hashtable_op_get(
                hashtable,
                test_key_1,
                test_key_1_len,
                &value);
    }

    hashtable_free(hashtable);
}


static void hashtable_op_get_found_key_external(benchmark::State& state) {
    hashtable_config_t* hashtable_config;
    hashtable_t* hashtable;
    hashtable_value_data_t value;

    hashtable_config = hashtable_config_init();
    hashtable_config->initial_size = state.range(0);
    hashtable_config->can_auto_resize = false;

    hashtable = hashtable_init(hashtable_config);

    HASHTABLE_BUCKET_HASH_KEY_VALUE_SET_KEY_EXTERNAL(
            test_index_1_buckets_count_53,
            test_key_1_hash,
            test_key_1,
            test_key_1_len,
            test_value_1);

    for (auto _ : state) {
        hashtable_op_get(
                hashtable,
                test_key_1,
                test_key_1_len,
                &value);
    }

    hashtable_free(hashtable);
}

BENCHMARK(hashtable_op_get_notfound)->HASHTABLE_OP_GET_BENCHS_ARGS;
BENCHMARK(hashtable_op_get_found_key_inline)->HASHTABLE_OP_GET_BENCHS_ARGS;
BENCHMARK(hashtable_op_get_found_key_external)->HASHTABLE_OP_GET_BENCHS_ARGS;
