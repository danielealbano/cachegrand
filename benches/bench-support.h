#ifndef BENCH_SUPPORT_H
#define BENCH_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#define BENCHES_MAX_THREADS_PER_CORE        4

void bench_support_collect_hashtable_stats_and_update_state(
    benchmark::State& state,
    hashtable_t* hashtable);

void bench_support_collect_hashtable_stats(
        hashtable_t* hashtable,
        uint64_t* return_used_chunks,
        double* return_load_factor_chunks,
        uint64_t* return_overflowed_chunks_count,
        double* return_used_avg_bucket_slots,
        uint64_t* return_used_max_overflowed_chunks_counter,
        hashtable_half_hashes_chunk_volatile_t** return_longest_half_hashes_chunk);

bool bench_support_check_if_too_many_threads_per_core(
        int threads,
        int max_per_core);

#ifdef __cplusplus
}
#endif

#endif //BENCH_SUPPORT_H
