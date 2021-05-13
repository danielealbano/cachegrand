/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#if defined(__linux__)
#include <sys/mman.h>
#else
#error Platform not supported
#endif

#include "log/log.h"
#include "misc.h"
#include "fatal.h"

#include "xalloc.h"

// TODO: should use libnuma set_mempolicy the be able to use mmap and allocate interleaved memory across numa nodes

#define MAP_HUGE_2MB    (21 << MAP_HUGE_SHIFT)   /* 2 MB huge pages */

#define TAG "xalloc"

void* xalloc_alloc(
        size_t size) {
    void* memptr;

    memptr = malloc(size);

    if (memptr == NULL) {
        FATAL(TAG, "Unable to allocate the requested memory %d", size);
    }

    return memptr;
}

void* xalloc_realloc(
        void* memptr,
        size_t size) {
    memptr = realloc(memptr, size);

    if (memptr == NULL) {
        FATAL(TAG, "Unable to allocate the requested memory %d to resize the pointer 0x%p", size, memptr);
    }

    return memptr;
}

void* xalloc_alloc_zero(
        size_t size) {
    void* memptr;

    memptr = xalloc_alloc(size);
    if (memset(memptr, 0, size) != memptr) {
        FATAL(TAG, "Unable to zero the requested memory %d", size);
    }

    return memptr;
}

void* xalloc_alloc_aligned(
        size_t alignment,
        size_t size) {
    void* memptr;
    bool failed = false;

#if defined(__linux__)
    memptr = aligned_alloc(alignment, size);

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

void* xalloc_alloc_aligned_zero(
        size_t alignment,
        size_t size) {
    void* memptr;

    memptr = xalloc_alloc_aligned(alignment, size);
    if (memset(memptr, 0, size) != memptr) {
        FATAL(TAG, "Unable to zero the requested memory %d", size);
    }

    return memptr;
}

void xalloc_free(
        void *memptr) {
    free(memptr);
}

size_t xalloc_get_page_size() {
    static size_t page_size;
    if (page_size > 0)
        return page_size;

#if defined(__linux__)
    page_size = getpagesize();
#else
#error Platform not supported
#endif

    return page_size;
}

void* xalloc_mmap_align_addr(
        void* memaddr) {
    long alignment = xalloc_get_page_size();

    memaddr -= 1;
    memaddr = memaddr - ((uintptr_t)memaddr % alignment) + alignment;

    return memaddr;
}

size_t xalloc_mmap_align_size(
        size_t size) {
    long alignment = xalloc_get_page_size();

    size -= 1;
    size = size - (size % alignment) + alignment;

    return size;
}

void* xalloc_mmap_alloc(
        size_t size) {
    void* memptr;
    bool failed = false;

    size = xalloc_mmap_align_size(size);

#if defined(__linux__)
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
#else
#error Platform not supported
#endif

    if (failed) {
        FATAL(TAG, "Unable to allocate the requested memory %d", size);
    }

    return memptr;
}

int xalloc_mmap_free(
        void *memptr,
        size_t size) {
#if defined(__linux__)
    return munmap(memptr, xalloc_mmap_align_size(size));
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

int xalloc_hugepages_free(
        void *memptr,
        size_t size) {
#if defined(__linux__)
    return munmap(memptr, xalloc_mmap_align_size(size));
#else
#error Platform not supported
#endif
}
