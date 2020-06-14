#include <string.h>

#include <benchmark/benchmark.h>

#include "cpu.h"
#include "log.h"
#include "hashtable/hashtable.h"

#include "bench-support.h"

const char* tag = "bench/support";

bool bench_support_check_if_too_many_threads_per_core(int threads, int max_threads_per_core) {
    return (threads / psnip_cpu_count()) > max_threads_per_core;
}

void bench_support_collect_hashtable_stats(
        hashtable_t* hashtable,
        uint64_t* return_used_buckets,
        double* return_load_factor_buckets,
        double* return_used_avg_bucket_slots,
        uint64_t* return_used_max_bucket_slots) {
    volatile hashtable_data_t* ht_data = hashtable->ht_current;

    uint64_t used_buckets = 0;
    double used_avg_bucket_slots = 0;
    uint64_t used_max_bucket_slots = 0;

    for(hashtable_chunk_index_t chunk_index = 0; chunk_index < ht_data->chunks_count; chunk_index++) {
        bool is_used_bucket = false;
        uint64_t used_current_bucket_slots = 0;

        for(
                hashtable_chunk_slot_index_t chunk_slot_index = 0;
                chunk_slot_index < HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT;
                chunk_slot_index++) {
            if (ht_data->half_hashes_chunk[chunk_index].half_hashes[chunk_slot_index] != 0) {
                used_avg_bucket_slots++;
                used_current_bucket_slots++;
                is_used_bucket = true;
            }
        }

        if (is_used_bucket) {
            used_buckets++;
        }

        if (used_current_bucket_slots > used_max_bucket_slots) {
            used_max_bucket_slots = used_current_bucket_slots;
            // TODO: use the overflowed_chunks_counter here
//            *return_longest_bucket = (hashtable_bucket_t*)bucket;
        }
    }

    *return_used_buckets = used_buckets;
    *return_used_max_bucket_slots = used_max_bucket_slots;
    *return_used_avg_bucket_slots = (double)used_avg_bucket_slots / (double)used_buckets;
    *return_load_factor_buckets = (double)used_buckets / (double)ht_data->buckets_count;
}

void bench_support_collect_hashtable_stats_and_update_state(benchmark::State& state, hashtable_t* hashtable) {
    uint64_t used_buckets;
    double load_factor_buckets;
    double used_avg_bucket_slots;
    uint64_t used_max_bucket_slots;
    bench_support_collect_hashtable_stats(
            hashtable,
            &used_buckets,
            &load_factor_buckets,
            &used_avg_bucket_slots,
            &used_max_bucket_slots);

    state.counters["total_buckets"] = state.range(0);
    state.counters["keys_to_insert"] = state.range(1);
    state.counters["load_factor"] = (double) state.range(1) / (double) state.range(0);
    state.counters["used_buckets"] = used_buckets;
    state.counters["load_factor_buckets"] = load_factor_buckets;
    state.counters["used_avg_bucket_slots"] = used_avg_bucket_slots;
    state.counters["used_max_bucket_slots"] = used_max_bucket_slots;
//
//    if (longest_bucket != nullptr) {
//        // HEADER
//        for (hashtable_chunk_slot_index_t i = 0; i < HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT; i++) {
//            hashtable_key_data_t *key;
//            hashtable_key_size_t key_len;
//            char *key_to_print;
//            char key_to_print_on_stack[100] = {0};
//
//            if (HASHTABLE_BUCKET_KEY_VALUE_HAS_FLAG(longest_bucket->keys_values[i].flags,
//                                                    HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE)) {
//                key = (char *) longest_bucket->keys_values[i].inline_key.data;
//                key_len = HASHTABLE_KEY_INLINE_MAX_LENGTH;
//
//                strncpy(key_to_print_on_stack, (const char *) key, key_len);
//                key_to_print = key_to_print_on_stack;
//            } else {
//#if defined(CACHEGRAND_HASHTABLE_KEY_CHECK_FULL)
//                key = longest_bucket->keys_values[i].->external_key.data;
//                key_len = longest_bucket->keys_values[i].->external_key.size;
//
//                key_to_print = key;
//#else
//                key = (char *) longest_bucket->keys_values[i].prefix_key.data;
//                key_len = HASHTABLE_KEY_PREFIX_SIZE;
//
//                strncpy(key_to_print_on_stack, (const char *) key, key_len);
//                key_to_print = key_to_print_on_stack;
//#endif // CACHEGRAND_HASHTABLE_KEY_CHECK_FULL
//            }
//
//            LOG_I(tag, "[%02d] HASH: 0x%08x | KEY: %s\n", i, longest_bucket->half_hashes[i], key_to_print);
//        }
//    }
}
