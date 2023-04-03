#ifndef CACHEGRAND_FFMA_REGION_CACHE_H
#define CACHEGRAND_FFMA_REGION_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

struct ffma_region_cache_numa_node {
    queue_mpmc_t *free_queue;
};
typedef struct ffma_region_cache_numa_node ffma_region_cache_numa_node_t;

struct ffma_region_cache {
    ffma_region_cache_numa_node_t *numa_nodes;
    size_t region_size;
    size_t cache_size;
    bool use_hugepages;
};
typedef struct ffma_region_cache ffma_region_cache_t;

/**
 * Initialize the region cache which is statically allocated and for the whole process and initialize the cache of
 * available regions for each NUMA node.
 *
 * @param region_size
 * @param cache_size
 * @param use_hugepages
 * @return
 */
ffma_region_cache_t* ffma_region_cache_init(
        size_t region_size,
        uint64_t cache_size,
        bool use_hugepages);

/**
 * Free the region cache which is statically allocated and for the whole process and free the cache of available regions
 */
void ffma_region_cache_free(
        ffma_region_cache_t* region_cache);

/**
 * Push a region to the cache of available regions for the current NUMA node, if the cache is full the memory is freed
 *
 * @param addr
 */
void ffma_region_cache_push(
        ffma_region_cache_t* region_cache,
        void* addr);

/**
 * Pop a region from the cache of available regions for the current NUMA node, if no regions are available NULL is
 * returned
 *
 * @return
 */
void* ffma_region_cache_pop(
        ffma_region_cache_t* region_cache);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FFMA_REGION_CACHE_H
