#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#if defined(__APPLE__) || defined(__linux__)
#include <sys/mman.h>
#elif defined(__MINGW32__)
#include <windows.h>
#endif

#include "fatal.h"

static const char* TAG = "xalloc_alloc";

void* xalloc_alloc(size_t size) {
    void* memptr;

    memptr = malloc(size);

    if (memptr == NULL) {
        FATAL(TAG, "Unable to allocate the requested memory %d", size);
    }

    return memptr;
}

void* xalloc_alloc_aligned(size_t alignment, size_t size) {
    void* memptr;

#if defined(__APPLE__)
    if (posix_memalign(&memptr, alignment, size) != 0) {
        FATAL(TAG, "Unable to allocate the requested memory %d aligned to %d", size, alignment);
    }
#elif defined(__linux__)
    memptr = aligned_alloc(alignment, size);

    if (memptr == NULL) {
        FATAL(TAG, "Unable to allocate the requested memory %d aligned to %d", size, alignment);
    }
#else
#error Platform not supported
#endif

    return memptr;
}

void xalloc_free(void *memptr) {
    free(memptr);
}

size_t xalloc_mmap_align_size(size_t size) {
    long alignment = sysconf(_SC_PAGESIZE);

    size = size - (size % alignment) + alignment;

    return size;
}

void* xalloc_mmap_alloc(size_t size) {
    void* memptr;

    size = xalloc_mmap_align_size(size);

    memptr = mmap(
            NULL,
            size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0);

    if (memptr < 0) {
        FATAL(TAG, "Unable to allocate the requested memory %d", size);
    }

    return memptr;
}

int xalloc_mmap_free(void *memptr, size_t size) {
    return munmap(memptr, xalloc_mmap_align_size(size));
}
