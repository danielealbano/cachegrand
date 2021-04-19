#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#if defined(__APPLE__) || defined(__linux__)
#include <sys/mman.h>
#elif defined(__MINGW32__)
#include <windows.h>
#endif

#include "log/log.h"
#include "misc.h"
#include "fatal.h"

#include "xalloc.h"

// TODO: should use libnuma set_mempolicy the be able to use mmap and allocate interleaved memory across numa nodes

#define MAP_HUGE_2MB    (21 << MAP_HUGE_SHIFT)   /* 2 MB huge pages */

#define TAG "xalloc"

void* xalloc_alloc(size_t size) {
    void* memptr;

    memptr = malloc(size);

    if (memptr == NULL) {
        FATAL(TAG, "Unable to allocate the requested memory %d", size);
    }

    return memptr;
}

void* xalloc_realloc(void* memptr, size_t size) {
    memptr = realloc(memptr, size);

    if (memptr == NULL) {
        FATAL(TAG, "Unable to allocate the requested memory %d to resize the pointer 0x%p", size, memptr);
    }

    return memptr;
}

void* xalloc_alloc_zero(size_t size) {
    void* memptr;

    memptr = xalloc_alloc(size);
    if (memset(memptr, 0, size) != memptr) {
        FATAL(TAG, "Unable to zero the requested memory %d", size);
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

void* xalloc_alloc_aligned_zero(size_t alignment, size_t size) {
    void* memptr;

    memptr = xalloc_alloc_aligned(alignment, size);
    if (memset(memptr, 0, size) != memptr) {
        FATAL(TAG, "Unable to zero the requested memory %d", size);
    }

    return memptr;
}

void xalloc_free(void *memptr) {
    free(memptr);
}

size_t xalloc_get_page_size() {
    static size_t page_size;
    if (page_size > 0)
        return page_size;

#if defined(__APPLE__) || defined(__linux__)
    page_size = getpagesize();
#elif defined(__MINGW32__)
    SYSTEM_INFO stInfo;
    GetNativeSystemInfo(&stInfo);
    page_size = stInfo.dwPageSize;
#else
#error Platform not supported
#endif

    return page_size;
}

void* xalloc_mmap_align_addr(void* memaddr) {
    long alignment = xalloc_get_page_size();

    memaddr -= 1;
    memaddr = memaddr - ((uintptr_t)memaddr % alignment) + alignment;

    return memaddr;
}

size_t xalloc_mmap_align_size(size_t size) {
    long alignment = xalloc_get_page_size();

    size -= 1;
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

void* xalloc_hugepages_2mb_alloc(
        size_t size) {
    void* memptr;

    // TODO: should be numa-aware, need to use set_mempolicy
#if defined(__linux__)
    size = xalloc_mmap_align_size(size);

    memptr = mmap(
            NULL,
            size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_2MB,
            -1,
            0);

    if (memptr == (void *)-1) {
        FATAL(TAG, "Unable to allocate the requested memory %d", size);
    }
#else
#error Platform not supported
#endif

    return memptr;
}

int xalloc_hugepages_free(void *memptr, size_t size) {
#if defined(__linux__)
    return munmap(memptr, xalloc_mmap_align_size(size));
#else
#error Platform not supported
#endif
}
