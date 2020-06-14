#include <stdio.h>
#include <string.h>
#include <benchmark/benchmark.h>

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_op_get.h"

#include "../tests/test-support.h"
#include "../tests/fixtures-hashtable.h"

#include "bench-support.h"

#define HASHTABLE_OP_GET_BENCHS_ARGS \
   Arg(1522U)->Arg(135798U)->Arg(1031398U)->Arg(17622551U)->Arg(89214403U) \
   ->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(16)->Threads(32)->Threads(64)->Threads(128)->Threads(256) \
   ->Iterations(10000000);

static void hashtable_op_get_not_found_key(benchmark::State& state) {
    hashtable_t* hashtable;
    hashtable_value_data_t value;

    hashtable = test_support_init_hashtable(state.range(0));

    test_support_set_thread_affinity(state.thread_index);

    for (auto _ : state) {
        hashtable_op_get(
                hashtable,
                test_key_1,
                test_key_1_len,
                &value);
    }

    hashtable_free(hashtable);
}

static void hashtable_op_get_single_key_inline(benchmark::State& state) {
    hashtable_t* hashtable;
    hashtable_value_data_t value;
    bool result;
    char error_message[150] = {0};

    hashtable = test_support_init_hashtable(state.range(0));

    hashtable_bucket_index_t bucket_index = test_key_1_hash % hashtable->ht_current->buckets_count;
    hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(bucket_index);
    hashtable_chunk_slot_index_t chunk_slot_index = 0;

    HASHTABLE_SET_KEY_INLINE_BY_INDEX(
            chunk_index,
            chunk_slot_index,
            test_key_1_hash,
            test_key_1,
            test_key_1_len,
            test_value_1);

    test_support_set_thread_affinity(state.thread_index);

    for (auto _ : state) {
        benchmark::DoNotOptimize((result = hashtable_op_get(
                hashtable,
                test_key_1,
                test_key_1_len,
                &value)));

        if (!result) {
            sprintf(
                    error_message,
                    "Unable to get the key <%s> with bucket index <%lu>, chunk index <%lu> and chunk slot index <%u> for the thread <%d>",
                    test_key_1,
                    bucket_index,
                    chunk_index,
                    chunk_slot_index,
                    state.thread_index);
            state.SkipWithError(error_message);
            break;
        }
    }

    hashtable_free(hashtable);
}


static void hashtable_op_get_single_key_external(benchmark::State& state) {
    hashtable_t* hashtable;
    hashtable_value_data_t value;
    bool result;
    char error_message[150] = {0};

    hashtable = test_support_init_hashtable(state.range(0));

    hashtable_bucket_index_t bucket_index = test_key_1_hash % hashtable->ht_current->buckets_count;
    hashtable_chunk_index_t chunk_index = HASHTABLE_TO_CHUNK_INDEX(bucket_index);
    hashtable_chunk_slot_index_t chunk_slot_index = 0;

    HASHTABLE_SET_KEY_INLINE_BY_INDEX(
            chunk_index,
            chunk_slot_index,
            test_key_1_hash,
            test_key_1,
            test_key_1_len,
            test_value_1);

    test_support_set_thread_affinity(state.thread_index);

    for (auto _ : state) {
        benchmark::DoNotOptimize((result = hashtable_op_get(
                hashtable,
                test_key_1,
                test_key_1_len,
                &value)));

        if (!result) {
            sprintf(
                    error_message,
                    "Unable to get the key <%s> with bucket index <%lu>, chunk index <%lu> and chunk slot index <%u> for the thread <%d>",
                    test_key_1,
                    bucket_index,
                    chunk_index,
                    chunk_slot_index,
                    state.thread_index);
            state.SkipWithError(error_message);
            break;
        }
    }

    hashtable_free(hashtable);
}

BENCHMARK(hashtable_op_get_not_found_key)->HASHTABLE_OP_GET_BENCHS_ARGS;
BENCHMARK(hashtable_op_get_single_key_inline)->HASHTABLE_OP_GET_BENCHS_ARGS;
BENCHMARK(hashtable_op_get_single_key_external)->HASHTABLE_OP_GET_BENCHS_ARGS;
