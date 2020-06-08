#ifndef BENCH_SUPPORT_H
#define BENCH_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

bool check_if_too_many_threads_per_core(int threads, int max_per_core);
void set_thread_affinity(int thread_index);

#ifdef __cplusplus
}
#endif

#endif //BENCH_SUPPORT_H
