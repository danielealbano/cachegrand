#ifndef CACHEGRAND_FFMA_PAGE_CACHE_H
#define CACHEGRAND_FFMA_PAGE_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

struct ffma_page_cache_numa_node {
    queue_mpmc_t *free_queue;
};
typedef struct ffma_page_cache_numa_node ffma_page_cache_numa_node_t;

struct ffma_page_cache {
    ffma_page_cache_numa_node_t *numa_nodes;
    size_t cache_size;
    bool use_hugepages;
};
typedef struct ffma_page_cache ffma_page_cache_t;

/**
 * Initialize the page cache which is statically allocated and for the whole process and initialize the cache of
 * available pages for each NUMA node.
 *
 * Although it's called "page cache" the cache contains actually 2MB of memory, if use_hugepages is true the memory
 * is allocated using hugepages, otherwise it's allocated using normal pages (e.g. 512 pages x 4KB on x86_64).
 *
 * @param cache_size
 * @param use_hugepages
 * @return
 */
ffma_page_cache_t* ffma_page_cache_init(
        uint64_t cache_size,
        bool use_hugepages);

/**
 * Free the page cache which is statically allocated and for the whole process and free the cache of available pages
 */
void ffma_page_cache_free();

/**
 * Push a page to the cache of available pages for the current NUMA node, if the cache is full the memory is freed
 *
 * @param addr
 */
void ffma_page_cache_push(
        void* addr);

/**
 * Pop a page from the cache of available pages for the current NUMA node, if no pages are available NULL is returned
 *
 * @return
 */
void* ffma_page_cache_pop();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FFMA_PAGE_CACHE_H
