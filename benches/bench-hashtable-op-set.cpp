#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <benchmark/benchmark.h>

#include "bench-support.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_op_set.h"
#include "xalloc.h"
#include "random.h"

#include "../tests/fixtures-hashtable.h"

#define BENCHES_MAX_THREADS_PER_CORE        4
#define RANDOM_KEYS_MIN_LENGTH              5
#define RANDOM_KEYS_MAX_LENGTH              30
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
    Threads(256)-> \
    Threads(512)-> \
    Threads(1024)

#define CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS(keys_gen_func_name) \
    UseRealTime()-> \
    SET_BENCH_ARGS_HT_SIZE_AND_KEYS(keys_gen_func_name)-> \
    SET_BENCH_ITERATIONS-> \
    SET_BENCH_THREADS

char* bench_build_keys_random_max_length(uint64_t count) {
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

char* bench_build_keys_random_random_length(uint64_t count) {
    char keys_character_set_list[] = {RANDOM_KEYS_CHARACTER_SET_LIST};
    char *keys = (char *) xalloc_mmap_alloc(count * RANDOM_KEYS_MAX_LENGTH);

    char *keys_current = keys;

    for (uint64_t i = 0; i < count; i++) {
        uint8_t i2;
        uint8_t length =
                ((random_generate() % (RANDOM_KEYS_MAX_LENGTH - RANDOM_KEYS_MIN_LENGTH)) + RANDOM_KEYS_MIN_LENGTH) -
                1;
        for (i2 = 0; i2 < length; i2++) {
            *keys_current = keys_character_set_list[random_generate() % RANDOM_KEYS_CHARACTER_SET_SIZE];
            keys_current++;
        }
        *keys_current = 0;
        keys_current += RANDOM_KEYS_MAX_LENGTH - length;

        assert((keys_current - keys) % RANDOM_KEYS_MAX_LENGTH != 0);
    }

    return keys;
}

void bench_free_keys(char* keys, uint64_t count) {
    xalloc_mmap_free(keys, count * RANDOM_KEYS_MAX_LENGTH);
}

void bench_collect_hashtable_stats(
        hashtable_t* hashtable,
        uint64_t* return_used_buckets,
        double* return_load_factor_buckets,
        double* return_used_avg_bucket_slots,
        uint64_t* return_used_max_bucket_slots) {
    volatile hashtable_data_t* ht_data = hashtable->ht_current;
    volatile hashtable_bucket_t* buckets = ht_data->buckets;

    uint64_t used_buckets = 0;
    double used_avg_bucket_slots = 0;
    uint64_t used_max_bucket_slots = 0;

    for(hashtable_bucket_index_t bucket_index = 0; bucket_index < ht_data->buckets_count; bucket_index++) {
        uint64_t used_current_bucket_slots = 0;
        volatile hashtable_bucket_t* bucket = &buckets[bucket_index];

        if (bucket->keys_values == nullptr) {
            continue;
        }

        used_buckets++;

        for(
                hashtable_bucket_slot_index_t bucket_slot_index = 0;
                bucket_slot_index < HASHTABLE_BUCKET_SLOTS_COUNT;
                bucket_slot_index++) {
            if (bucket->half_hashes[bucket_slot_index] != 0) {
                used_avg_bucket_slots++;
                used_current_bucket_slots++;
            }
        }

        if (used_current_bucket_slots > used_max_bucket_slots) {
            used_max_bucket_slots = used_current_bucket_slots;
        }
    }

    *return_used_buckets = used_buckets;
    *return_used_max_bucket_slots = used_max_bucket_slots;
    *return_used_avg_bucket_slots = (double)used_avg_bucket_slots / (double)used_buckets;
    *return_load_factor_buckets = (double)used_buckets / (double)ht_data->buckets_count;
}

void bench_collect_hashtable_stats_and_update_state(benchmark::State& state, hashtable_t* hashtable) {
    uint64_t used_buckets;
    double load_factor_buckets;
    double used_avg_bucket_slots;
    uint64_t used_max_bucket_slots;
    bench_collect_hashtable_stats(
            hashtable,
            &used_buckets,
            &load_factor_buckets,
            &used_avg_bucket_slots,
            &used_max_bucket_slots);

    state.counters["total_buckets"] = state.range(0);
    state.counters["keys_to_insert"] = state.range(1);
    state.counters["load_factor"] = (double)state.range(1) / (double)state.range(0);
    state.counters["used_buckets"] = used_buckets;
    state.counters["load_factor_buckets"] = load_factor_buckets;
    state.counters["used_avg_bucket_slots"] = used_avg_bucket_slots;
    state.counters["used_max_bucket_slots"] = used_max_bucket_slots;
}

static void hashtable_op_set_new(benchmark::State& state) {
    static hashtable_config_t* hashtable_config;
    static hashtable_t* hashtable;
    static char* keys;
    char error_message[150] = {0};

    if (check_if_too_many_threads_per_core(state.threads, BENCHES_MAX_THREADS_PER_CORE)) {
        sprintf(error_message, "Too many threads per core, max allowed <%d>", BENCHES_MAX_THREADS_PER_CORE);
        state.SkipWithError(error_message);
        return;
    }

    if (state.thread_index == 0) {
        if (state.range(2) == RANDOM_KEYS_GEN_FUNC_MAX_LENGTH) {
            keys = bench_build_keys_random_max_length(state.range(0));
        } else {
            keys = bench_build_keys_random_random_length(state.range(0));
        }

        hashtable_config = hashtable_config_init();
        hashtable_config->initial_size = state.range(0);
        hashtable_config->can_auto_resize = false;

        hashtable = hashtable_init(hashtable_config);
    }

    set_thread_affinity(state.thread_index);

    for (auto _ : state) {
        for(long int i = state.thread_index; i < state.range(1); i += state.threads) {
            char* key = keys + (RANDOM_KEYS_MAX_LENGTH * i);
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
        bench_collect_hashtable_stats_and_update_state(state, hashtable);
        hashtable_free(hashtable);
        bench_free_keys(keys, state.range(0));
    }
}

static void hashtable_op_set_update(benchmark::State& state) {
    static hashtable_config_t* hashtable_config;
    static hashtable_t* hashtable;
    static char* keys;
    char error_message[150] = {0};

    if (check_if_too_many_threads_per_core(state.threads, BENCHES_MAX_THREADS_PER_CORE)) {
        sprintf(error_message, "Too many threads per core, max allowed <%d>", BENCHES_MAX_THREADS_PER_CORE);
        state.SkipWithError(error_message);
        return;
    }

    if (state.thread_index == 0) {
        if (state.range(2) == RANDOM_KEYS_GEN_FUNC_MAX_LENGTH) {
            keys = bench_build_keys_random_max_length(state.range(0));
        } else {
            keys = bench_build_keys_random_random_length(state.range(0));
        }

        hashtable_config = hashtable_config_init();
        hashtable_config->initial_size = state.range(0);
        hashtable_config->can_auto_resize = false;
        hashtable = hashtable_init(hashtable_config);

        for(long int i = 0; i < state.range(1); i++) {
            char* key = keys + (RANDOM_KEYS_MAX_LENGTH * i);

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

    set_thread_affinity(state.thread_index);

    for (auto _ : state) {
        for(long int i = state.thread_index; i < state.range(1); i += state.threads) {
            char* key = keys + (RANDOM_KEYS_MAX_LENGTH * i);

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
        bench_collect_hashtable_stats_and_update_state(state, hashtable);
        hashtable_free(hashtable);
        bench_free_keys(keys, state.range(0));
    }
}

BENCHMARK(hashtable_op_set_new)
    ->CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS(RANDOM_KEYS_GEN_FUNC_RANDOM_LENGTH);
BENCHMARK(hashtable_op_set_update)
    ->CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS(RANDOM_KEYS_GEN_FUNC_RANDOM_LENGTH);

