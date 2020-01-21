#ifndef CACHEGRAND_XALLOC_H
#define CACHEGRAND_XALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

size_t xalloc_page_align(size_t size);
void* xalloc_aligned(size_t alignment, size_t size);
void* xalloc(size_t size);
void xfree(void* memptr);
void* xalloc_hugepages(size_t size);
int xfree_hugepages(void* memptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_XALLOC_H
