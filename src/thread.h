#ifndef CACHEGRAND_THREAD_H
#define CACHEGRAND_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

void thread_flush_cached_core_index_and_numa_node_index();

long thread_current_get_id();

uint32_t thread_current_set_affinity(
        uint32_t thread_index);

int thread_affinity_set_selected_cpus_sort(
        const void * a,
        const void * b);

void thread_affinity_set_selected_cpus(
        uint16_t* selected_cpus,
        uint16_t selected_cpus_count);

uint8_t thread_get_current_numa_node_index();

uint16_t thread_get_current_core_index();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_THREAD_H
