#ifndef CACHEGRAND_UTILS_NUMA_H
#define CACHEGRAND_UTILS_NUMA_H

#ifdef __cplusplus
extern "C" {
#endif

bool utils_numa_is_available();
int utils_numa_node_configured_count();
uint32_t utils_numa_node_current_index();
int utils_numa_cpu_configured_count();
bool utils_numa_cpu_allowed(
        uint32_t cpu_index);
uint32_t utils_numa_cpu_current_index();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_UTILS_NUMA_H
