#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <benchmark/benchmark.h>

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_support_index.h"
#include "hashtable/hashtable_op_set.h"
#include "hashtable/hashtable_op_delete.h"

#include "../tests/fixtures-hashtable.h"

#define HASHTABLE_OP_SET_BENCHS_ARGS \
    Args({1522, 250})->Args({135798, 22500})->Args({1031398, 175000})

static void hashtable_op_set_new(benchmark::State& state) {
    static hashtable_config_t* hashtable_config;
    static hashtable_t* hashtable;
    static char** keys;
    hashtable_value_data_t value;

    if (state.thread_index == 0) {
        hashtable_config = hashtable_config_init();
        hashtable_config->initial_size = state.range(0);
        hashtable_config->can_auto_resize = false;

        hashtable = hashtable_init(hashtable_config);

        keys = (char**)malloc(sizeof(char*) * state.range(1));
        for(int i = 0; i < state.range(1); i++) {
            keys[i] = (char*)malloc(20);
            sprintf(keys[i], "%d", i);
        }
    }

    for (auto _ : state) {
        for(int i = 0; i < state.range(1); i++) {
            hashtable_op_set(
                    hashtable,
                    keys[i],
                    strlen(keys[i]),
                    test_value_1);
        }

        state.PauseTiming();

        for(int i = 0; i < state.range(1); i++) {
            hashtable_op_delete(
                    hashtable,
                    keys[i],
                    strlen(keys[i]));
        }

        state.ResumeTiming();
    }

    if (state.thread_index == 0) {
        hashtable_free(hashtable);

        for(int i = 0; i < state.range(1); i++) {
            free(keys[i]);
        }
        free(keys);
    }
}


static void hashtable_op_set_update(benchmark::State& state) {
    static hashtable_config_t* hashtable_config;
    static hashtable_t* hashtable;
    static char** keys;
    hashtable_value_data_t value;

    if (state.thread_index == 0) {
        hashtable_config = hashtable_config_init();
        hashtable_config->initial_size = state.range(0);
        hashtable_config->can_auto_resize = false;

        hashtable = hashtable_init(hashtable_config);

        keys = (char**)malloc(sizeof(char*) * state.range(1));
        for(int i = 0; i < state.range(1); i++) {
            keys[i] = (char*)malloc(20);
            sprintf(keys[i], "%d", i);
        }

        for(int i = 0; i < state.range(1); i++) {
            hashtable_op_set(
                    hashtable,
                    keys[i],
                    strlen(keys[i]),
                    test_value_1);
        }
    }

    for (auto _ : state) {
        for(int i = 0; i < state.range(1); i++) {
            hashtable_op_set(
                    hashtable,
                    keys[i],
                    strlen(keys[i]),
                    test_value_1);
        }
    }

    if (state.thread_index == 0) {
        hashtable_free(hashtable);

        for(int i = 0; i < state.range(1); i++) {
            free(keys[i]);
        }
        free(keys);
    }
}

BENCHMARK(hashtable_op_set_new)->HASHTABLE_OP_SET_BENCHS_ARGS->Threads(1)->Threads(2)->Threads(4);
BENCHMARK(hashtable_op_set_update)->HASHTABLE_OP_SET_BENCHS_ARGS->Threads(1)->Threads(2)->Threads(4);
