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
#include "random.h"

#include "../tests/fixtures-hashtable.h"

#define RANDOM_KEYS_MIN_LENGTH              5
#define RANDOM_KEYS_MAX_LENGTH              30
#define RANDOM_KEYS_TO_PREGENERATE_COUNT    200732527U
#define RANDOM_KEYS_CHARACTER_SET_LIST      'q','w','e','r','t','y','u','i','o','p','a','s','d','f','g','h','j','k', \
                                            'l','z','x','c','v','b','n','m', \
                                            'q','w','e','r','t','y','u','i','o','p','a','s','d','f','g','h','j','k', \
                                            'l','z','x','c','v','b','n','m', \
                                            'Q','W','E','R','T','Y','U','I','O','P','A','S','D','F','G','H','J','K', \
                                            'L','Z','X','C','V','B','N','M', \
                                            'Q','W','E','R','T','Y','U','I','O','P','A','S','D','F','G','H','J','K', \
                                            'L','Z','X','C','V','B','N','M', \
                                            '1','2','3','4','5','6','7','8','9','0', \
                                            '1','2','3','4','5','6','7','8','9','0', \
                                            '.',',','/','|','\'',';',']','[','<','>','?',']',':','"','|','{','}','!',\
                                            '@','$','%','^','&','*','(',')','_','-','=','+','#'
#define RANDOM_KEYS_CHARACTER_SET_SIZE      sizeof((char[]){RANDOM_KEYS_CHARACTER_SET_LIST})
#define RANDOM_KEYS_GEN_FUNC_MAX_LENGTH     1
#define RANDOM_KEYS_GEN_FUNC_RANDOM_LENGTH  2


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

#define LOAD_FACTOR_BENCH_ARGS(keys_gen_func_name) \
    Args({42U, keys_gen_func_name})-> \
    Args({101U, keys_gen_func_name})-> \
    Args({307U, keys_gen_func_name})-> \
    Args({677U, keys_gen_func_name})-> \
    Args({1523U, keys_gen_func_name})-> \
    Args({3389U, keys_gen_func_name})-> \
    Args({7639U, keys_gen_func_name})-> \
    Args({17203U, keys_gen_func_name})-> \
    Args({26813U, keys_gen_func_name})-> \
    Args({40213U, keys_gen_func_name})-> \
    Args({60353U, keys_gen_func_name})-> \
    Args({90529U, keys_gen_func_name})-> \
    Args({135799U, keys_gen_func_name})-> \
    Args({203669U, keys_gen_func_name})-> \
    Args({305581U, keys_gen_func_name})-> \
    Args({458377U, keys_gen_func_name})-> \
    Args({687581U, keys_gen_func_name})-> \
    Args({1031399U, keys_gen_func_name})-> \
    Args({1547101U, keys_gen_func_name})-> \
    Args({2320651U, keys_gen_func_name})-> \
    Args({5221501U, keys_gen_func_name})-> \
    Args({7832021U, keys_gen_func_name})-> \
    Args({11748391U, keys_gen_func_name})-> \
    Args({17622551U, keys_gen_func_name})-> \
    Args({26433887U, keys_gen_func_name})-> \
    Args({39650833U, keys_gen_func_name})-> \
    Args({59476253U, keys_gen_func_name})-> \
    Args({89214403U, keys_gen_func_name})-> \
    Args({133821599U, keys_gen_func_name})-> \
    Args({200732527U, keys_gen_func_name})

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

#define CONFIGURE_LOAD_FACTOR_BENCH(keys_gen_func_name)  \
    LOAD_FACTOR_BENCH_ARGS(keys_gen_func_name)-> \
    Iterations(1)-> \
    ReportAggregatesOnly(false)

#define handle_error_en(en, msg) \
               do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

char* build_keys_random_max_length(uint64_t count) {
    char keys_character_set_list[] = { RANDOM_KEYS_CHARACTER_SET_LIST };
    char* keys = (char*)xalloc_mmap_alloc(count * RANDOM_KEYS_MAX_LENGTH);

    char* keys_current = keys;
    for(uint64_t i = 0; i < count; i++) {
        for(uint8_t i2 = 0; i2 < RANDOM_KEYS_MAX_LENGTH - 1; i2++) {
            *keys_current = keys_character_set_list[random_generate() % RANDOM_KEYS_CHARACTER_SET_SIZE];
            keys_current++;
        }
        *keys_current=0;
        keys_current++;

        assert((keys_current - keys) % RANDOM_KEYS_MAX_LENGTH != 0);
    }

    return keys;
}

char* build_keys_random_random_length(uint64_t count) {
    char keys_character_set_list[] = { RANDOM_KEYS_CHARACTER_SET_LIST };
    char* keys = (char*)xalloc_mmap_alloc(count * RANDOM_KEYS_MAX_LENGTH);

    char* keys_current = keys;

    for(uint64_t i = 0; i < count; i++) {
        uint8_t i2;
        uint8_t length =  ((random_generate() % (RANDOM_KEYS_MAX_LENGTH - RANDOM_KEYS_MIN_LENGTH)) + RANDOM_KEYS_MIN_LENGTH) - 1;
        for(i2 = 0; i2 < length; i2++) {
            *keys_current = keys_character_set_list[random_generate() % RANDOM_KEYS_CHARACTER_SET_SIZE];
            keys_current++;
        }
        *keys_current=0;
        keys_current += RANDOM_KEYS_MAX_LENGTH - length;

        assert((keys_current - keys) % RANDOM_KEYS_MAX_LENGTH != 0);
    }

    return keys;
}

void free_keys(char* keys, uint64_t count) {
    xalloc_mmap_free(keys, count * RANDOM_KEYS_MAX_LENGTH);
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
        hashtable_config->cachelines_to_probe = cachelines_to_probe_2;

        hashtable = hashtable_init(hashtable_config);
        keys = build_keys_random_max_length(state.range(1));
    }

    set_thread_affinity(state.thread_index);

    for (auto _ : state) {
        for(int i = 0; i < state.range(1); i++) {
            char* key = keys + (RANDOM_KEYS_MAX_LENGTH * i);
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
        hashtable_config->cachelines_to_probe = cachelines_to_probe_2;

        hashtable = hashtable_init(hashtable_config);
        keys = build_keys_random_max_length(state.range(1));

        for(int i = 0; i < state.range(1); i++) {
            char* key = keys + (RANDOM_KEYS_MAX_LENGTH * i);

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
            char* key = keys + (RANDOM_KEYS_MAX_LENGTH * i);

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
        if (state.range(1) == RANDOM_KEYS_GEN_FUNC_MAX_LENGTH) {
            keys = build_keys_random_max_length(RANDOM_KEYS_TO_PREGENERATE_COUNT);
        } else {
            keys = build_keys_random_random_length(RANDOM_KEYS_TO_PREGENERATE_COUNT);
        }
    }

    for (auto _ : state) {
        do {
            hashtable_config = hashtable_config_init();
            hashtable_config->initial_size = state.range(0) - 1;
            hashtable_config->can_auto_resize = false;
            hashtable_config->cachelines_to_probe = cachelines_to_probe;
            hashtable = hashtable_init(hashtable_config);

            uint64_t buckets_count = hashtable->ht_current->buckets_count;
            state.counters["buckets_count"] = buckets_count;

            set_thread_affinity(1);
            uint64_t inserted_keys_counter = 0;

            // HACK: Invoking directly ResumeTimeing will have the side effect of resetting the internal execution time
            // and not to "sum" it because PauseTiming is not being invoked.
            state.ResumeTiming();

            for(int i = 0; i < buckets_count; i++) {
                bool result;
                char* key = keys + (RANDOM_KEYS_MAX_LENGTH * i);
                benchmark::DoNotOptimize(result = hashtable_op_set(
                        hashtable,
                        key,
                        strlen(key),
                        test_value_1));

                if (!result) {
                    break;
                }

                inserted_keys_counter++;
            }

            if (inserted_keys_counter == 0) {
                state.counters["inserted_keys"] = 0;
                state.counters["load_factor"] = 0;
                state.counters["cachelines_to_probe"] = 0;
            } else {
                state.counters["inserted_keys"] = inserted_keys_counter;
                state.counters["load_factor"] = (double)inserted_keys_counter / (double)buckets_count;
                state.counters["cachelines_to_probe"] = cachelines_to_probe;
            }

            if (state.counters["load_factor"] >= 0.74) {
                state.PauseTiming();
                hashtable_free(hashtable);
                state.ResumeTiming();
                break;
            } else {
                hashtable_free(hashtable);

                fprintf(stdout, "Load factor %f lower than 0.75 with %d cache lines, re-trying with %d cache lines\n",
                        (double)state.counters["load_factor"],
                        cachelines_to_probe,
                        cachelines_to_probe + 1);
                fflush(stdout);
                cachelines_to_probe += 1;
            }
        }
        while(true);
    }
}

BENCHMARK(hashtable_op_set_new)->CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS;
BENCHMARK(hashtable_op_set_update)->CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS;
BENCHMARK(hashtable_op_set_load_factor)->CONFIGURE_LOAD_FACTOR_BENCH;



BENCHMARK(hashtable_op_set_load_factor)
    ->CONFIGURE_LOAD_FACTOR_BENCH(RANDOM_KEYS_GEN_FUNC_RANDOM_LENGTH);

