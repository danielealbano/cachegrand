#ifndef CACHEGRAND_HASHTABLE_THREAD_COUNTERS_H
#define CACHEGRAND_HASHTABLE_THREAD_COUNTERS_H

#ifdef __cplusplus
extern "C" {
#endif

hashtable_counters_t *hashtable_mcmp_thread_counters_sum_fetch(
        hashtable_t *hashtable);

void hashtable_mcmp_thread_counters_sum_free(
        hashtable_counters_t *counters_sum);

hashtable_counters_volatile_t* hashtable_mcmp_thread_counters_get_current_thread(
        hashtable_t* hashtable);

void hashtable_mcmp_thread_counters_reset(
        hashtable_data_volatile_t *hashtable_data);

void hashtable_mcmp_thread_counters_expand_to(
        hashtable_data_volatile_t *hashtable_data,
        uint32_t new_size);

uint32_t hashtable_mcmp_thread_counters_fetch_new_index(
        hashtable_data_volatile_t *hashtable_data);

hashtable_counters_volatile_t* hashtable_mcmp_thread_counters_get_by_index(
        hashtable_data_volatile_t *hashtable_data,
        uint32_t index);

void hashtable_mcmp_thread_counters_init(
        hashtable_data_volatile_t *hashtable_data,
        uint32_t initial_size);

void hashtable_mcmp_thread_counters_free(
        hashtable_data_volatile_t *hashtable_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_THREAD_COUNTERS_H
