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
    bool failed = false;

#if defined(__APPLE__)
    if (posix_memalign(&memptr, alignment, size) != 0) {
        failed = true;
    }
#elif defined(__linux__)
    memptr = aligned_alloc(alignment, size);

    if (memptr == NULL) {
        failed = true;
    }
#elif defined(__MINGW32__)
    memptr = _aligned_malloc(size, alignment);

    if (memptr == NULL) {
        failed = true;
    }
#else
#error Platform not supported
#endif

    if (failed) {
        FATAL(TAG, "Unable to allocate the requested memory %d aligned to %d", size, alignment);
    }

    return memptr;
}

void xalloc_free(void *memptr) {
    free(memptr);
}

size_t xalloc_mmap_align_size(size_t size) {
    long alignment;

#if defined(__APPLE__) || defined(__linux__)
    alignment = sysconf(_SC_PAGESIZE);
#elif defined(__MINGW32__)
    SYSTEM_INFO stInfo;
    GetNativeSystemInfo(&stInfo);
    alignment = stInfo.dwPageSize;
#else
#error Platform not supported
#endif

    size = size - (size % alignment) + alignment;

    return size;
}

void* xalloc_mmap_alloc(size_t size) {
    void* memptr;
    bool failed = false;

    size = xalloc_mmap_align_size(size);

#if defined(__APPLE__) || defined(__linux__)
    memptr = mmap(
            NULL,
            size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0);

    if (memptr == (void *)-1) {
        failed = true;
    }
#elif defined(__MINGW32__)
    // Reference implementation https://github.com/witwall/mman-win32/blob/master/mman.c
    const DWORD dwMaxSizeLow = (DWORD)(size & 0xFFFFFFFFL);
    const DWORD dwMaxSizeHigh = (DWORD)((size >> 32) & 0xFFFFFFFFL);

    HANDLE fhm = CreateFileMapping(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            dwMaxSizeHigh,
            dwMaxSizeLow,
            NULL);

    if (fhm == NULL) {
        FATAL(TAG, "Unable to create the file mapping to allocate the requested memory", size);
    }

    memptr = MapViewOfFile(
            fhm,
            (DWORD)FILE_MAP_READ | (DWORD)FILE_MAP_WRITE,
            0,
            0,
            0);

    CloseHandle(fhm);

    if (memptr == NULL) {
        failed = true;
    }

#else
#error Platform not supported
#endif

    if (failed) {
        FATAL(TAG, "Unable to allocate the requested memory %d", size);
    }

    return memptr;
}

int xalloc_mmap_free(void *memptr, size_t size) {
#if defined(__APPLE__) || defined(__linux__)
    return munmap(memptr, xalloc_mmap_align_size(size));
#elif defined(__MINGW32__)
    if (UnmapViewOfFile(memptr)) {
        return 0;
    }

    return -1;
#else
#error Platform not supported
#endif
}
