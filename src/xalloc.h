#ifndef CACHEGRAND_XALLOC_H
#define CACHEGRAND_XALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

void* xalloc_aligned(size_t alignment, size_t size);
void* xalloc(size_t size);
void xfree(void* memptr);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_XALLOC_H
