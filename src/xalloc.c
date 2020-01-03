#include <stdlib.h>

#include "log.h"
#include "fatal.h"

static const char* TAG = "xalloc";

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
