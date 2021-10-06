#ifndef CACHEGRAND_HUGEPAGE_CACHE_H
#define CACHEGRAND_HUGEPAGE_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hugepage_cache hugepage_cache_t;
struct hugepage_cache {
    int numa_node_index;
    spinlock_lock_t lock;
    double_linked_list_t* free_hugepages;
    struct {
        uint32_t total;
        uint32_t in_use;
    } stats;
};

hugepage_cache_t* hugepage_cache_init();

void hugepage_cache_free();

void* hugepage_cache_push(
        void* hugepage_addr);

void* hugepage_cache_pop();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HUGEPAGE_CACHE_H
