#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <benchmark/benchmark.h>

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_support_index.h"
#include "hashtable/hashtable_op_set.h"
#include "hashtable/hashtable_op_delete.h"
#include "xalloc.h"

#include "../tests/fixtures-hashtable.h"

#define KEY_MAX_LENGTH              12
#define KEYS_TO_PREGENERATE_COUNT   677472371U

#define SET_BENCH_ARGS_HT_SIZE_AND_KEYS \
    Args({1522, 1522 / 4})-> \
    Args({1522, 1522 / 3})-> \
    Args({1522, 1522 / 2})-> \
    Args({135798, 135798 / 4})-> \
    Args({135798, 135798 / 3})-> \
    Args({135798, 135798 / 2})-> \
    Args({1031398, 1031398 / 4})-> \
    Args({1031398, 1031398 / 3})-> \
    Args({1031398, 1031398 / 2})-> \
    Args({11748391, 11748391U / 4})-> \
    Args({11748391, 11748391U / 3})-> \
    Args({11748391, 11748391U / 2})-> \
    Args({133821673, 133821673 / 4})-> \
    Args({133821673, 133821673 / 3})-> \
    Args({133821673, 133821673 / 2})

#define LOAD_FACTOR_BENCH_ARGS \
    Arg(42U)-> \
    Arg(101U)-> \
    Arg(307U)-> \
    Arg(677U)-> \
    Arg(1523U)-> \
    Arg(3389U)-> \
    Arg(7639U)-> \
    Arg(17203U)-> \
    Arg(26813U)-> \
    Arg(40213U)-> \
    Arg(60353U)-> \
    Arg(90529U)-> \
    Arg(135799U)-> \
    Arg(203669U)-> \
    Arg(305581U)-> \
    Arg(458377U)-> \
    Arg(687581U)-> \
    Arg(1031399U)-> \
    Arg(1547101U)-> \
    Arg(2320651U)-> \
    Arg(5221501U)-> \
    Arg(7832021U)-> \
    Arg(11748391U)-> \
    Arg(17622551U)-> \
    Arg(26433887U)-> \
    Arg(39650833U)-> \
    Arg(59476253U)-> \
    Arg(89214403U)-> \
    Arg(133821599U)-> \
    Arg(200732527U)-> \
    Arg(301099033U)-> \
    Arg(451649113U)-> \
    Arg(677472127U)-> \
    Arg(1016208581U)-> \
    Arg(1524312899U)-> \
    Arg(2286469357U)-> \
    Arg(3429704039U)-> \
    Arg(4294967291U)

#define SET_BENCH_ITERATIONS \
    Iterations(2)

#define SET_BENCH_THREADS \
    Threads(1)-> \
    Threads(2)-> \
    Threads(4)-> \
    Threads(8)-> \
    Threads(16)

#define CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS \
    UseRealTime()-> \
    SET_BENCH_ARGS_HT_SIZE_AND_KEYS-> \
    SET_BENCH_ITERATIONS-> \
    SET_BENCH_THREADS

#define CONFIGURE_LOAD_FACTOR_BENCH \
    UseRealTime()-> \
    LOAD_FACTOR_BENCH_ARGS-> \
    Iterations(1)

#define handle_error_en(en, msg) \
               do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

void shuffle(uint64_t *array, size_t n) {
    if (n > 1) {
        size_t i;
        for (i = 0; i < n - 1; i++) {
            size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
            int t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}

char* build_keys_random(uint64_t count) {
    fprintf(stdout, "Generating random keys...\n"); fflush(stdout);

    uint64_t* keys_numbers = (uint64_t*)xalloc_mmap_alloc(count * sizeof(uint64_t));
    char* keys = (char*)xalloc_mmap_alloc(count * KEY_MAX_LENGTH);

    for(uint64_t i = 0; i < count; i++) {
        keys_numbers[i] = i;
    }

    shuffle(keys_numbers, count);

    for(uint64_t i = 0; i< count; i++) {
        sprintf(keys + (KEY_MAX_LENGTH * i), "%ld", keys_numbers[i]);
    }

    xalloc_mmap_free(keys_numbers, count * sizeof(uint64_t));

    fprintf(stdout, "Random keys generated\n"); fflush(stdout);

    return keys;
}

void free_keys(char* keys, uint64_t count) {
    xalloc_mmap_free(keys, count * KEY_MAX_LENGTH);
}

void set_thread_affinity(int thread_index) {
    int res;
    cpu_set_t cpuset;
    pthread_t thread;

    CPU_ZERO(&cpuset);
    CPU_SET(thread_index, &cpuset);

    thread = pthread_self();
    res = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (res != 0) {
        handle_error_en(res, "pthread_setaffinity_np");
    }
}

static void hashtable_op_set_new(benchmark::State& state) {
    static hashtable_config_t* hashtable_config;
    static hashtable_t* hashtable;
    static char* keys;

    if (state.thread_index == 0) {
        hashtable_config = hashtable_config_init();
        hashtable_config->initial_size = state.range(0);
        hashtable_config->can_auto_resize = false;

        hashtable = hashtable_init(hashtable_config);
        keys = build_keys_random(state.range(1));
    }

    set_thread_affinity(state.thread_index);

    for (auto _ : state) {
        for(int i = 0; i < state.range(1); i++) {
            char* key = keys + (KEY_MAX_LENGTH * i);
            hashtable_op_set(
                    hashtable,
                    key,
                    strlen(key),
                    test_value_1);
        }
    }

    if (state.thread_index == 0) {
        hashtable_free(hashtable);
        free_keys(keys, state.range(1));
    }
}

static void hashtable_op_set_update(benchmark::State& state) {
    static hashtable_config_t* hashtable_config;
    static hashtable_t* hashtable;
    static char* keys;

    if (state.thread_index == 0) {
        hashtable_config = hashtable_config_init();
        hashtable_config->initial_size = state.range(0);
        hashtable_config->can_auto_resize = false;

        hashtable = hashtable_init(hashtable_config);
        keys = build_keys_random(state.range(1));

        for(int i = 0; i < state.range(1); i++) {
            char* key = keys + (KEY_MAX_LENGTH * i);

            hashtable_op_set(
                    hashtable,
                    key,
                    strlen(key),
                    test_value_1);
        }
    }

    set_thread_affinity(state.thread_index);

    for (auto _ : state) {
        for(int i = state.thread_index; i < state.range(1); i += state.threads) {
            char* key = keys + (KEY_MAX_LENGTH * i);

            hashtable_op_set(
                    hashtable,
                    key,
                    strlen(key),
                    test_value_1);
        }
    }

    if (state.thread_index == 0) {
        hashtable_free(hashtable);
        free_keys(keys, state.range(1));
    }
}

static void hashtable_op_set_load_factor(benchmark::State& state) {
    static hashtable_config_t* hashtable_config;
    static hashtable_t* hashtable;
    static char* keys = nullptr;

    static uint16_t cachelines_to_probe = 1;

    if (keys == nullptr) {
        keys = build_keys_random(KEYS_TO_PREGENERATE_COUNT);
    }

    for (auto _ : state) {
        do {
            state.PauseTiming();

            hashtable_config = hashtable_config_init();
            hashtable_config->initial_size = state.range(0) - 1;
            hashtable_config->can_auto_resize = false;
            hashtable_config->cachelines_to_probe = cachelines_to_probe;
            hashtable = hashtable_init(hashtable_config);

            uint64_t buckets_count = hashtable->ht_current->buckets_count;
            state.counters["buckets_count"] = buckets_count;

            set_thread_affinity(1);
            uint64_t inserted_keys_counter = buckets_count + 1;
            state.ResumeTiming();

            uint64_t inserted_keys_counter_temp = 0;
            for(int i = 0; i < buckets_count; i++) {
                char* key = keys + (KEY_MAX_LENGTH * i);
                bool result = hashtable_op_set(
                        hashtable,
                        key,
                        strlen(key),
                        test_value_1);

                if (!result) {
                    break;
                }

                inserted_keys_counter_temp++;
            }

            if (inserted_keys_counter > inserted_keys_counter_temp) {
                inserted_keys_counter = inserted_keys_counter_temp;
            }

            state.PauseTiming();

            hashtable_free(hashtable);

            if (inserted_keys_counter > buckets_count) {
                state.counters["inserted_keys"] = 0;
                state.counters["load_factor"] = 0;
                state.counters["cachelines_to_probe"] = 0;
            } else {
                state.counters["inserted_keys"] = inserted_keys_counter;
                state.counters["load_factor"] = (double)inserted_keys_counter / (double)buckets_count;
                state.counters["cachelines_to_probe"] = cachelines_to_probe;
            }

            if (state.counters["load_factor"] > 0.75) {
                state.ResumeTiming();
                break;
            } else {
                cachelines_to_probe += 1;
                state.ResumeTiming();
            }
        }
        while(true);
    }
}

BENCHMARK(hashtable_op_set_new)->CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS;
BENCHMARK(hashtable_op_set_update)->CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS;
BENCHMARK(hashtable_op_set_load_factor)->CONFIGURE_LOAD_FACTOR_BENCH;




