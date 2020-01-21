#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "log.h"
#include "fatal.h"

static const char* TAG = "xalloc";

size_t xalloc_page_align(size_t size) {
    long alignment = sysconf(_SC_PAGESIZE);

    size_t hugepages_alignment = alignment; //2 * 1024 * 1024;
    size = size - (size % hugepages_alignment) + hugepages_alignment;

    return size;
}

void* xalloc_aligned(size_t alignment, size_t size) {
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

void* xalloc(size_t size) {
    void* memptr;

    memptr = malloc(size);

    if (memptr == NULL) {
        FATAL(TAG, "Unable to allocate the requested memory %d", size);
    }

    return memptr;
}

void xfree(void* memptr) {
    free(memptr);
}

void* xalloc_hugepages(size_t size) {
    void* memptr;

    size = xalloc_page_align(size);

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

    if (madvise(
            memptr,
            size,
            MADV_RANDOM | MADV_HUGEPAGE) == -1) {
        FATAL(TAG, "Unable to advise the kernel for the data at %p with size %ld", memptr, size);
    }

    return memptr;
}

int xfree_hugepages(void* memptr, size_t size) {
    return munmap(memptr, xalloc_page_align(size));
}
