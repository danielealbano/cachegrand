#ifndef CACHEGRAND_XALLOC_H
#define CACHEGRAND_XALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

enum xalloc_mmap_try_alloc_fixed_addr_result {
    XALLOC_MMAP_TRY_ALLOC_FIXED_ADDR_RESULT_FAILED_UNKNOWN,
    XALLOC_MMAP_TRY_ALLOC_FIXED_ADDR_RESULT_FAILED_NO_FREE_MEM,
    XALLOC_MMAP_TRY_ALLOC_FIXED_ADDR_RESULT_FAILED_ALREADY_ALLOCATED,
    XALLOC_MMAP_TRY_ALLOC_FIXED_ADDR_RESULT_FAILED_DIFFERENT_ADDR,
    XALLOC_MMAP_TRY_ALLOC_FIXED_ADDR_RESULT_SUCCESS,
};
typedef enum xalloc_mmap_try_alloc_fixed_addr_result xalloc_mmap_try_alloc_fixed_addr_result_t;

__attribute__((malloc))
void* xalloc_alloc(
        size_t size);

void* xalloc_realloc(
        void* memptr,
        size_t size);

__attribute__((malloc))
void* xalloc_alloc_zero(
        size_t size);

__attribute__((malloc))
void* xalloc_alloc_aligned(
        size_t alignment,
        size_t size);

__attribute__((malloc))
void* xalloc_alloc_aligned_zero(
        size_t alignment,
        size_t size);

void xalloc_free(
        void *memptr);

size_t xalloc_get_page_size();

void* xalloc_mmap_align_addr(
        void* memaddr);

size_t xalloc_mmap_align_size(
        size_t size);

void* xalloc_random_aligned_addr(
        size_t alignment,
        size_t size);

__attribute__((malloc))
void* xalloc_mmap_alloc(
        size_t size);

xalloc_mmap_try_alloc_fixed_addr_result_t xalloc_mmap_try_alloc_fixed_addr(
        void *requested_addr,
        size_t size,
        bool use_hugepages,
        void **out_addr);

int xalloc_mmap_free(
        void *memptr,
        size_t size);

__attribute__((malloc))
void* xalloc_hugepage_alloc(
        size_t size);

int xalloc_hugepage_free(
        void *memptr,
        size_t size);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_XALLOC_H
