/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <cstdio>
#include <cstring>

#include <benchmark/benchmark.h>

#include "exttypes.h"
#include "spinlock.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_op_get.h"

#include "../tests/support.h"
#include "../tests/hashtable/fixtures-hashtable.h"

#include "benchmark-program.hpp"
#include "benchmark-support.hpp"

static void hashtable_op_get_not_found_key(benchmark::State& state) {
    static hashtable_t* hashtable;
    hashtable_value_data_t value;

    if (state.thread_index() == 0) {
        hashtable = test_support_init_hashtable(state.range(0));
    }

    test_support_set_thread_affinity(state.thread_index());

    for (auto _ : state) {
        benchmark::DoNotOptimize(hashtable_mcmp_op_get(
                hashtable,
                test_key_1,
                test_key_1_len,
                &value));
    }

    if (state.thread_index() == 0) {
        hashtable_mcmp_free(hashtable);
    }
}

#if HASHTABLE_FLAG_ALLOW_KEY_INLINE == 1
static void hashtable_op_get_single_key_inline(benchmark::State& state) {
    static hashtable_t* hashtable;
    static hashtable_bucket_index_t bucket_index;
    static hashtable_chunk_index_t chunk_index;
    static hashtable_chunk_slot_index_t chunk_slot_index;
    hashtable_value_data_t value;
    bool result;
    char error_message[150] = {0};

    if (state.thread_index() == 0) {
        hashtable = test_support_init_hashtable(state.range(0));

        bucket_index = test_key_1_hash % hashtable->ht_current->buckets_count;
        chunk_index = HASHTABLE_TO_CHUNK_INDEX(bucket_index);
        chunk_slot_index = 0;

        HASHTABLE_SET_KEY_INLINE_BY_INDEX(
                chunk_index,
                chunk_slot_index,
                test_key_1_hash,
                test_key_1,
                test_key_1_len,
                test_value_1);
    }

    test_support_set_thread_affinity(state.thread_index());

    for (auto _ : state) {
        benchmark::DoNotOptimize((result = hashtable_mcmp_op_get(
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
                    state.thread_index());
            state.SkipWithError(error_message);
            break;
        }
    }

    if (state.thread_index() == 0) {
        hashtable_mcmp_free(hashtable);
    }
}
#endif

static void hashtable_op_get_single_key_external(benchmark::State& state) {
    static hashtable_t* hashtable;
    static hashtable_bucket_index_t bucket_index;
    static hashtable_chunk_index_t chunk_index;
    static hashtable_chunk_slot_index_t chunk_slot_index;
    hashtable_value_data_t value;
    bool result;
    char error_message[150] = {0};

    if (state.thread_index() == 0) {
        hashtable = test_support_init_hashtable(state.range(0));

        bucket_index = test_key_1_hash % hashtable->ht_current->buckets_count;
        chunk_index = HASHTABLE_TO_CHUNK_INDEX(bucket_index);
        chunk_slot_index = 0;

        HASHTABLE_SET_KEY_EXTERNAL_BY_INDEX(
                chunk_index,
                chunk_slot_index,
                test_key_1_hash,
                test_key_1,
                test_key_1_len,
                test_value_1);
    }

    test_support_set_thread_affinity(state.thread_index());

    for (auto _ : state) {
        benchmark::DoNotOptimize((result = hashtable_mcmp_op_get(
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
                    state.thread_index());
            state.SkipWithError(error_message);
            break;
        }
    }

    if (state.thread_index() == 0) {
        hashtable_mcmp_free(hashtable);
    }
}

BENCHMARK(hashtable_op_get_not_found_key)
        ->ArgsProduct({
                              { 0x0000FFFFu, 0x000FFFFFu, 0x001FFFFFu, 0x007FFFFFu, 0x00FFFFFFu, 0x01FFFFFFu, 0x07FFFFFFu,
                                0x0FFFFFFFu, 0x1FFFFFFFu, 0x3FFFFFFFu, 0x7FFFFFFFu },
                              { 50, 75 },
                      })
        ->ThreadRange(1, utils_cpu_count())
        ->Iterations(10000000);

#if HASHTABLE_FLAG_ALLOW_KEY_INLINE == 1
BENCHMARK(hashtable_op_get_single_key_inline)
        ->ArgsProduct({
                              { 0x0000FFFFu, 0x000FFFFFu, 0x001FFFFFu, 0x007FFFFFu, 0x00FFFFFFu, 0x01FFFFFFu, 0x07FFFFFFu,
                                0x0FFFFFFFu, 0x1FFFFFFFu, 0x3FFFFFFFu, 0x7FFFFFFFu },
                              { 50, 75 },
                      })
        ->ThreadRange(1, utils_cpu_count())
        ->Iterations(10000000);

#endif

BENCHMARK(hashtable_op_get_single_key_external)
        ->ArgsProduct({
                              { 0x0000FFFFu, 0x000FFFFFu, 0x001FFFFFu, 0x007FFFFFu, 0x00FFFFFFu, 0x01FFFFFFu, 0x07FFFFFFu,
                                0x0FFFFFFFu, 0x1FFFFFFFu, 0x3FFFFFFFu, 0x7FFFFFFFu },
                              { 50, 75 },
                      })
        ->ThreadRange(1, utils_cpu_count())
        ->Iterations(10000000);
