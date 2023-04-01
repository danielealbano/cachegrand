/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#ifndef DISABLE_MIMALLOC
#define DISABLE_MIMALLOC 0
#endif

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#if defined(__linux__)
#include <sys/mman.h>
#include <assert.h>
#include <errno.h>

#else
#error Platform not supported
#endif

#include "misc.h"
#include "clock.h"
#include "log/log.h"
#include "fatal.h"
#include "random.h"

#if DISABLE_MIMALLOC == 0
#include "mimalloc.h"
#else
#define mi_malloc malloc
#define mi_realloc realloc
#define mi_zalloc(size) (calloc(1, size))
#define mi_free free
#define mi_malloc_aligned(SIZE, ALIGNMENT) (aligned_alloc(ALIGNMENT, SIZE))
#define mi_zalloc_aligned(SIZE, ALIGNMENT) (aligned_alloc(ALIGNMENT, SIZE))
#endif

#include "xalloc.h"

#define MAP_HUGE_2MB    (21 << MAP_HUGE_SHIFT)   /* 2 MB hugepages */

#define TAG "xalloc"

void* xalloc_alloc(
        size_t size) {
    void* memptr;

    memptr = mi_malloc(size);

    if (memptr == NULL) {
        FATAL(TAG, "Unable to allocate the requested memory %lu", size);
    }

    return memptr;
}

void* xalloc_realloc(
        void* memptr,
        size_t size) {
    memptr = mi_realloc(memptr, size);

    if (memptr == NULL) {
        FATAL(TAG, "Unable to allocate the requested memory %lu to resize the pointer 0x%p", size, memptr);
    }

    return memptr;
}

void* xalloc_alloc_zero(
        size_t size) {
    void* memptr;

    memptr = mi_zalloc(size);

#if DISABLE_MIMALLOC == 1
    memset(memptr, 0, size);
#endif

    if (memptr == NULL) {
        FATAL(TAG, "Unable to allocate the requested memory %lu", size);
    }

    if (memset(memptr, 0, size) != memptr) {
        FATAL(TAG, "Unable to zero the requested memory %lu", size);
    }

    return memptr;
}

void* xalloc_alloc_aligned(
        size_t alignment,
        size_t size) {
    void* memptr;
    bool failed = false;

    memptr = mi_malloc_aligned(size, alignment);

    if (memptr == NULL) {
        failed = true;
    }

    if (failed) {
        FATAL(TAG, "Unable to allocate the requested memory %lu aligned to %lu", size, alignment);
    }

    return memptr;
}

void* xalloc_alloc_aligned_zero(
        size_t alignment,
        size_t size) {
    void* memptr;

    memptr = mi_zalloc_aligned(size, alignment);

#if DISABLE_MIMALLOC == 1
    memset(memptr, 0, size);
#endif

    if (memptr == NULL) {
        FATAL(TAG, "Unable to allocate the requested memory %lu", size);
    }

    if (memset(memptr, 0, size) != memptr) {
        FATAL(TAG, "Unable to zero the requested memory %lu", size);
    }

    return memptr;
}

void xalloc_free(
        void *memptr) {
    mi_free(memptr);
}

size_t xalloc_get_page_size() {
    static size_t page_size;

    if (page_size > 0) {
        return page_size;
    }

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

void* xalloc_random_aligned_addr(
        size_t alignment) {
#if defined(__linux__)
#if __aarch64__
#define XALLOC_ADDR_ALLOWED_BITS (0x7FFFFFFFFF)
#elif __x86_64__
#define XALLOC_ADDR_ALLOWED_BITS (0x7FFFFFFFFFFF)
#else
#error Platform not supported
#endif
#else
#error Platform not supported
#endif

    uintptr_t random_addr = random_generate();
    return (void*)((random_addr - (random_addr % alignment)) & XALLOC_ADDR_ALLOWED_BITS);
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
        FATAL(TAG, "Unable to allocate the requested memory %lu", size);
    }

    return memptr;
}

xalloc_mmap_try_alloc_fixed_addr_result_t xalloc_mmap_try_alloc_fixed_addr(
        void *requested_addr,
        size_t size,
        void **out_addr) {
    xalloc_mmap_try_alloc_fixed_addr_result_t result = XALLOC_MMAP_TRY_ALLOC_FIXED_ADDR_RESULT_SUCCESS;

    assert((uintptr_t)requested_addr % xalloc_get_page_size() == 0);

    size = xalloc_mmap_align_size(size);

#if defined(__linux__)
    *out_addr = mmap(
            requested_addr,
            size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
            -1,
            0);

    if (unlikely(*out_addr == (void*)-1)) {
        if (errno == EEXIST) {
            result = XALLOC_MMAP_TRY_ALLOC_FIXED_ADDR_RESULT_FAILED_ALREADY_ALLOCATED;
        } else if (errno == ENOMEM) {
            result = XALLOC_MMAP_TRY_ALLOC_FIXED_ADDR_RESULT_FAILED_NO_FREE_MEM;
        } else {
            result = XALLOC_MMAP_TRY_ALLOC_FIXED_ADDR_RESULT_FAILED_UNKNOWN;
        }
    }

#if DEBUG == 1
    // Not really needed, only pre 4.17 kernels will return a different address but cachegrand requires a 5.7 or newer
    // kernel but it's better to have a check here to be sure. The check though is only done in debug mode to avoid
    // a performance hit in production.
    if (*out_addr != (void*)-1 && *out_addr != requested_addr) {
        result = XALLOC_MMAP_TRY_ALLOC_FIXED_ADDR_RESULT_FAILED_DIFFERENT_ADDR;
        munmap(*out_addr, size);
    }
#endif

#else
#error Platform not supported
#endif

    return result;
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

void* xalloc_hugepage_alloc(
        size_t size) {
    void* memptr;

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
        LOG_E(TAG, "Unable to allocate the hugepage of size %lu", size);
        return NULL;
    }
#else
#error Platform not supported
#endif

    return memptr;
}

int xalloc_hugepage_free(
        void *memptr,
        size_t size) {
#if defined(__linux__)
    return munmap(memptr, xalloc_mmap_align_size(size));
#else
#error Platform not supported
#endif
}
