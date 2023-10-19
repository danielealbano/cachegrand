/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#ifndef TRACK_ALLOCATIONS
#define TRACK_ALLOCATIONS 
#endif

#if TRACK_ALLOCATIONS == 1
#define DISABLE_MIMALLOC 1
#endif

#ifndef DISABLE_MIMALLOC
#define DISABLE_MIMALLOC 0
#endif

#if DISABLE_MIMALLOC == 1
#if TRACK_ALLOCATIONS == 1
#define __USE_GNU
#define _GNU_SOURCE
#endif
#endif

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

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
#include <stdlib.h>
#include <malloc.h>
#define mi_malloc(size) ({ xalloc_track_allocations_in_xalloc_func = true; void *new_ptr = malloc(size); xalloc_track_allocations_in_xalloc_func = false; new_ptr; })
#define mi_realloc(memptr, size) ({ xalloc_track_allocations_in_xalloc_func = true; void *new_ptr = realloc(memptr, size); xalloc_track_allocations_in_xalloc_func = false; new_ptr; })
#define mi_zalloc(size) ({ xalloc_track_allocations_in_xalloc_func = true; void *new_ptr = calloc(1, size); xalloc_track_allocations_in_xalloc_func = false; new_ptr; })
#define mi_free(ptr) ({ xalloc_track_allocations_in_xalloc_func = true; free(ptr); xalloc_track_allocations_in_xalloc_func = false; })
#define mi_malloc_aligned(size, alignment) ({ xalloc_track_allocations_in_xalloc_func = true; void *new_ptr = memalign(alignment, size); xalloc_track_allocations_in_xalloc_func = false; new_ptr; })
#define mi_zalloc_aligned(size, alignment) ({ xalloc_track_allocations_in_xalloc_func = true; void *new_ptr = memalign(alignment, size); xalloc_track_allocations_in_xalloc_func = false; new_ptr; })
#endif

#if DISABLE_MIMALLOC == 1
#if TRACK_ALLOCATIONS == 1
#include <stdatomic.h>
#include <unistd.h>
#include <dlfcn.h>
#include <backtrace-supported.h>
#include <backtrace.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include "intrinsics.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"

#define XALLOC_TRACK_ALLOCATIONS_PER_THREAD_QUEUE_SIZE 4096

enum xalloc_track_allocations_entry_type {
    XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MALLOC,
    XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_CALLOC,
    XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_REALLOC,
    XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_FREE,
    XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MEMALIGN,
    XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MMAP,
    XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MUNMAP,
};
typedef enum xalloc_track_allocations_entry_type xalloc_track_allocations_entry_type_t;

struct xalloc_track_allocations_entry {
    xalloc_track_allocations_entry_type_t type;
    void* ptr;
    void* caller;
    size_t nmemb;
    size_t size;
    size_t alignment;
    void* return_ptr;
    uint64_t timestamp;
};
typedef struct xalloc_track_allocations_entry xalloc_track_allocations_entry_t;
typedef _Volatile(xalloc_track_allocations_entry_t) xalloc_track_allocations_entry_volatile_t;

struct xalloc_track_allocations_per_thread_queue {
    pthread_t thread;
    pid_t thread_id;
    uint32_t size;
    uint32_t mask;
    uint32_volatile_t head;
    uint32_volatile_t tail;
    xalloc_track_allocations_entry_volatile_t entries[];
};
typedef struct xalloc_track_allocations_per_thread_queue xalloc_track_allocations_per_thread_queue_t;
typedef _Volatile(xalloc_track_allocations_per_thread_queue_t) xalloc_track_allocations_per_thread_queue_volatile_t;

static void xalloc_track_allocations_allocate_per_thread_queue();
static void xalloc_track_allocations_malloc_wrapper(size_t size, void* ptr, void* caller);
static void xalloc_track_allocations_calloc_wrapper(size_t nmemb, size_t size, void* ptr, void* caller);
static void xalloc_track_allocations_realloc_wrapper(void *ptr, size_t size, void* ptrnew, void* caller);
static void xalloc_track_allocations_free_wrapper(void* ptr, void* caller);
static void xalloc_track_allocations_memalign_wrapper(size_t alignment, size_t size, void* ptr, void* caller);

thread_local bool_volatile_t xalloc_track_allocations_in_xalloc_func = false;
#endif
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
    static bool page_size_read;
    static size_t page_size;

    if (unlikely(!page_size_read)) {
        page_size_read = true;
#if defined(__linux__)
        page_size = getpagesize();
#else
#error Platform not supported
#endif
    }


    return page_size;
}

void* xalloc_mmap_align_addr(
        void* memaddr) {
    size_t alignment = xalloc_get_page_size();

    memaddr -= 1;
    memaddr = memaddr - ((uintptr_t)memaddr % alignment) + alignment;

    return memaddr;
}

size_t xalloc_mmap_align_size(
        size_t size) {
    size_t alignment = xalloc_get_page_size();

    size -= 1;
    size = size - (size % alignment) + alignment;

    return size;
}

void* xalloc_random_aligned_addr(
        size_t alignment,
        size_t size) {
#if defined(__linux__)
#if __aarch64__
    size_t max_addr = 0x7FFFFFFFFF;
    size_t min_addr = 0x1000000000;
#elif __x86_64__
    size_t max_addr = 0x7FFFFFFFFFFF;
    size_t min_addr = 0x20000000000;
#else
#error Platform not supported
#endif
#else
#error Platform not supported
#endif

    // Calculates a random address in range between the allowed one, ensures it's far enough to be able to allocate size
    // and aligns it to the requested alignment
    uintptr_t random_addr = (random_generate() % (max_addr - (min_addr + size))) - size;
    uintptr_t aligned_random_addr = random_addr - (random_addr % alignment);

    return (void*)aligned_random_addr;
}

void* xalloc_mmap_alloc(
        size_t size) {
    void* memptr;
    bool failed = false;

#if TRACK_ALLOCATIONS == 1
    xalloc_track_allocations_in_xalloc_func = true;
#endif

    size = xalloc_mmap_align_size(size);

#if defined(__linux__)
    memptr = mmap(
            NULL,
            size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0);

    if (memptr == MAP_FAILED) {
        failed = true;
    }
#else
#error Platform not supported
#endif

    if (failed) {
        FATAL(TAG, "Unable to allocate the requested memory %lu", size);
    }

#if TRACK_ALLOCATIONS == 1
    xalloc_track_allocations_in_xalloc_func = false;
#endif

    return memptr;
}

xalloc_mmap_try_alloc_fixed_addr_result_t xalloc_mmap_try_alloc_fixed_addr(
        void *requested_addr,
        size_t size,
        bool use_hugepages,
        void **out_addr) {
    xalloc_mmap_try_alloc_fixed_addr_result_t result = XALLOC_MMAP_TRY_ALLOC_FIXED_ADDR_RESULT_SUCCESS;

    assert((uintptr_t)requested_addr % xalloc_get_page_size() == 0);

#if TRACK_ALLOCATIONS == 1
    xalloc_track_allocations_in_xalloc_func = true;
#endif

    size = xalloc_mmap_align_size(size);

#if defined(__linux__)
    *out_addr = mmap(
            requested_addr,
            size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE | (use_hugepages ? MAP_HUGETLB | MAP_HUGE_2MB : 0),
            -1,
            0);

    if (unlikely(*out_addr == MAP_FAILED)) {
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

#if TRACK_ALLOCATIONS == 1
    xalloc_track_allocations_in_xalloc_func = false;
#endif

    return result;
}

int xalloc_mmap_free(
        void *memptr,
        size_t size) {
    int result;

#if TRACK_ALLOCATIONS == 1
    xalloc_track_allocations_in_xalloc_func = true;
#endif

#if defined(__linux__)
    result = munmap(memptr, xalloc_mmap_align_size(size));
#else
#error Platform not supported
#endif

#if TRACK_ALLOCATIONS == 1
    xalloc_track_allocations_in_xalloc_func = false;
#endif

    return result;
}

void* xalloc_hugepage_alloc(
        size_t size) {
    void* memptr;

#if TRACK_ALLOCATIONS == 1
    xalloc_track_allocations_in_xalloc_func = true;
#endif

#if defined(__linux__)
    size = xalloc_mmap_align_size(size);

    memptr = mmap(
            NULL,
            size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_2MB,
            -1,
            0);

    if (memptr == MAP_FAILED) {
        LOG_E(TAG, "Unable to allocate the hugepage of size %lu", size);
        memptr = NULL;
    }
#else
#error Platform not supported
#endif

#if TRACK_ALLOCATIONS == 1
    xalloc_track_allocations_in_xalloc_func = false;
#endif

    return memptr;
}

int xalloc_hugepage_free(
        void *memptr,
        size_t size) {
    int result;

#if TRACK_ALLOCATIONS == 1
    xalloc_track_allocations_in_xalloc_func = true;
#endif

#if defined(__linux__)
    result = munmap(memptr, xalloc_mmap_align_size(size));
#else
#error Platform not supported
#endif

#if TRACK_ALLOCATIONS == 1
    xalloc_track_allocations_in_xalloc_func = false;
#endif

    return result;
}

#if DISABLE_MIMALLOC == 1
#if TRACK_ALLOCATIONS == 1
extern void *__libc_malloc(size_t size);
extern void *__libc_memalign(size_t alignment, size_t size);
extern void *__libc_calloc(size_t nmemb, size_t size);
extern void *__libc_realloc(void *ptr, size_t size);
extern void __libc_free(void *ptr);
extern void * __mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int __munmap(void *addr, size_t length);

void * __libc_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return __mmap(addr, length, prot, flags, fd, offset);
}

int __libc_munmap(void *addr, size_t length) {
    return __munmap(addr, length);
}

bool_volatile_t xalloc_track_allocations_ctor_thread_terminate = false;
bool_volatile_t xalloc_track_allocations_ctor_thread_terminated = false;
thread_local bool_volatile_t xalloc_track_allocations_in_hook = false;

thread_local bool_volatile_t xalloc_track_allocations_per_thread_queue_allocated = false;
thread_local xalloc_track_allocations_per_thread_queue_volatile_t *xalloc_track_allocations_per_thread_queue;

static uint16_volatile_t xalloc_track_allocations_registered_per_thread_queue_count = 0;
static spinlock_lock_t xalloc_track_allocations_registered_per_thread_queue_spinlock = { 0 };
static xalloc_track_allocations_per_thread_queue_volatile_t **xalloc_track_allocations_registered_per_thread_queue = NULL;

static pthread_t xalloc_track_allocations_thread;

#define XALLOC_TRACK_ALLOCATIONS_HOOK_RETURN_PTR(return_type, func, signature, ...) \
    return_type func signature {                                             \
        if (unlikely(xalloc_track_allocations_in_hook)) {              \
            return __libc_ ## func(__VA_ARGS__);                       \
        }                                                              \
        xalloc_track_allocations_in_hook = true;                       \
        if (unlikely(!xalloc_track_allocations_per_thread_queue_allocated)) { \
            xalloc_track_allocations_allocate_per_thread_queue();      \
        }                                                              \
        return_type return_ptr = __libc_ ## func(__VA_ARGS__);               \
        xalloc_track_allocations_ ## func ## _wrapper(__VA_ARGS__, return_ptr, xalloc_track_allocations_allocate_get_caller()); \
        xalloc_track_allocations_in_hook = false;                      \
        return return_ptr;                                             \
    }

#define XALLOC_TRACK_ALLOCATIONS_HOOK_NO_RETURN(func, signature, ...) \
    void func signature {                                          \
        if (unlikely(xalloc_track_allocations_in_hook)) {              \
            __libc_ ## func(__VA_ARGS__);                          \
            return;                                                \
        }                                                          \
        xalloc_track_allocations_in_hook = true;                   \
        if (unlikely(!xalloc_track_allocations_per_thread_queue_allocated)) { \
            xalloc_track_allocations_allocate_per_thread_queue();  \
        }                                                          \
        __libc_ ## func(__VA_ARGS__);                              \
        xalloc_track_allocations_ ## func ## _wrapper(__VA_ARGS__, xalloc_track_allocations_allocate_get_caller()); \
        xalloc_track_allocations_in_hook = false;                  \
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
// Implementation for a ring bounded spsc queue, specialized for the xalloc_track_allocations_entry_t type to ensure
// maximum performance. It also operates directly on the queue entries avoid memory copies entirely.
static void xalloc_track_allocations_allocate_per_thread_queue() {
    // Allocate the new per thread queue
    xalloc_track_allocations_per_thread_queue = (xalloc_track_allocations_per_thread_queue_volatile_t*)calloc(1,
            sizeof(xalloc_track_allocations_per_thread_queue_volatile_t) +
            (sizeof(xalloc_track_allocations_entry_volatile_t) * XALLOC_TRACK_ALLOCATIONS_PER_THREAD_QUEUE_SIZE));
    xalloc_track_allocations_per_thread_queue->thread_id = gettid();
    xalloc_track_allocations_per_thread_queue->thread = pthread_self();
    xalloc_track_allocations_per_thread_queue->size = XALLOC_TRACK_ALLOCATIONS_PER_THREAD_QUEUE_SIZE;
    xalloc_track_allocations_per_thread_queue->mask = XALLOC_TRACK_ALLOCATIONS_PER_THREAD_QUEUE_SIZE - 1;

    // Register the new queue
    spinlock_lock(&xalloc_track_allocations_registered_per_thread_queue_spinlock);
    xalloc_track_allocations_per_thread_queue_volatile_t **new_list = realloc(xalloc_track_allocations_registered_per_thread_queue,
            sizeof(xalloc_track_allocations_per_thread_queue_volatile_t*) * (xalloc_track_allocations_registered_per_thread_queue_count + 1));

    if (!new_list) {
        FATAL(TAG, "Unable to allocate memory for the new list of registered per thread queues");
    }

    xalloc_track_allocations_registered_per_thread_queue = new_list;
    xalloc_track_allocations_registered_per_thread_queue[xalloc_track_allocations_registered_per_thread_queue_count] =
            xalloc_track_allocations_per_thread_queue;
    xalloc_track_allocations_registered_per_thread_queue_count++;
    spinlock_unlock(&xalloc_track_allocations_registered_per_thread_queue_spinlock);

    // Mark the queue as allocated
    xalloc_track_allocations_per_thread_queue_allocated = true;
}
#pragma clang diagnostic pop

struct backtrace_state *__bt_state = NULL;

int xalloc_track_allocations_allocate_get_caller_callback(void *data, uintptr_t pc) {
    // Save the caller in data
    void **caller = data;
    *caller = (void*)pc;

    // Stop the backtrace
    return 1;
}

void xalloc_track_allocations_allocate_get_caller_callback_error(void *, const char *msg, int errnum) {
    fprintf(stderr, "Error %d occurred while trying to get the caller: %s", errnum, msg);
    fflush(stderr);
}

void xalloc_track_allocations_create_state_callback_error(void *, const char *msg, int errnum) {
    fprintf(stderr, "Error %d occurred while trying to get the caller info: %s", errnum, msg);
    fflush(stderr);
}

void xalloc_track_allocations_acquire_create_state() {
    char current_executable[1024];
    size_t current_executable_len = readlink("/proc/self/exe", current_executable, sizeof(current_executable));
    if (current_executable_len == -1) {
        FATAL(TAG, "xalloc track allocations: failed to readlink /proc/self/exe");
    }
    current_executable[current_executable_len] = '\0';

    __bt_state = backtrace_create_state(
            current_executable,
            1,
            xalloc_track_allocations_create_state_callback_error,
            NULL);
}

static void* xalloc_track_allocations_allocate_get_caller() {
    void *caller = NULL;
    uint8_t skip;
    if (xalloc_track_allocations_in_xalloc_func) {
        skip = 3;
    } else {
        skip = 2;
    }

//    if (__bt_state == NULL) {
//        xalloc_track_allocations_acquire_create_state();
//    }
//
//    backtrace_simple(
//            __bt_state,
//            skip,
//            xalloc_track_allocations_allocate_get_caller_callback,
//            xalloc_track_allocations_allocate_get_caller_callback_error,
//            &caller);

    unw_cursor_t cursor;
    unw_context_t context;

    // grab the machine context and initialize the cursor
    if (unw_getcontext(&context) < 0) {
        FATAL(TAG, "cannot get local machine state");
    }

    if (unw_init_local(&cursor, &context) < 0) {
        FATAL(TAG, "cannot initialize cursor for local unwinding");
    }

    unw_word_t pc = 0;

    // unwind until we reach the desired frame
    skip++;
    do {
        skip--;
        if (skip > 0) {
            continue;
        }

        if (unw_get_reg(&cursor, UNW_REG_IP, &pc)) {
            LOG_E(TAG, "Cannot read program counter");
            break;
        }
    } while (unw_step(&cursor) > 0 && skip > 0);
    caller = (void*)pc;

    return caller;
}

static inline uint64_t xalloc_track_allocations_allocate_per_thread_queue_get_next_free_slot_index(
        xalloc_track_allocations_per_thread_queue_volatile_t *queue,
        bool *found) {
    MEMORY_FENCE_LOAD();
    bool full = (queue->tail - queue->head) ==
            queue->size;
    if (unlikely(full)) {
        *found = false;
        return 0;
    }

    *found = true;
    return queue->tail & queue->mask;
}

static inline uint64_t xalloc_track_allocations_allocate_per_thread_queue_get_next_free_slot_index_wait(
        xalloc_track_allocations_per_thread_queue_volatile_t *queue) {
    bool found = false;
    uint64_t index;

    do {
        found = false;
        index = xalloc_track_allocations_allocate_per_thread_queue_get_next_free_slot_index(queue, &found);
    } while(!found);

    return index;
}

static inline xalloc_track_allocations_entry_volatile_t *xalloc_track_allocations_allocate_per_thread_queue_get_slot_ptr(
        xalloc_track_allocations_per_thread_queue_volatile_t *queue,
        uint64_t index) {
    return &queue->entries[index];
}

static inline xalloc_track_allocations_entry_volatile_t* xalloc_track_allocations_allocate_per_thread_queue_get_next_free_slot_ptr_wait(
        xalloc_track_allocations_per_thread_queue_volatile_t *queue) {
    return xalloc_track_allocations_allocate_per_thread_queue_get_slot_ptr(
            queue,
            xalloc_track_allocations_allocate_per_thread_queue_get_next_free_slot_index_wait(queue));
}

static inline void xalloc_track_allocations_allocate_per_thread_queue_mark_enqueued(
        xalloc_track_allocations_per_thread_queue_volatile_t *queue) {
    MEMORY_FENCE_STORE();
    queue->tail++;
    MEMORY_FENCE_STORE();
}

static inline uint64_t xalloc_track_allocations_allocate_per_thread_queue_get_next_full_slot_index(
        xalloc_track_allocations_per_thread_queue_volatile_t *queue,
        bool *found) {
    MEMORY_FENCE_LOAD();
    bool empty = queue->head == queue->tail;
    if (unlikely(empty)) {
        *found = false;
        return 0;
    }

    *found = true;
    return queue->head & queue->mask;
}

static inline void xalloc_track_allocations_allocate_per_thread_queue_mark_dequeued(
        xalloc_track_allocations_per_thread_queue_volatile_t *queue) {
    MEMORY_FENCE_STORE();
    queue->head++;
    MEMORY_FENCE_STORE();
}

static void xalloc_track_allocations_malloc_wrapper(size_t size, void *return_ptr, void *caller) {
    assert(return_ptr != NULL);

    xalloc_track_allocations_entry_volatile_t *entry =
            xalloc_track_allocations_allocate_per_thread_queue_get_next_free_slot_ptr_wait(
                    xalloc_track_allocations_per_thread_queue);

    entry->timestamp = intrinsics_tsc();
    entry->return_ptr = return_ptr;
    entry->caller = caller;

    entry->type = XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MALLOC;
    entry->size = size;

    xalloc_track_allocations_allocate_per_thread_queue_mark_enqueued(
            xalloc_track_allocations_per_thread_queue);
}

static void xalloc_track_allocations_calloc_wrapper(size_t nmemb, size_t size, void *return_ptr, void *caller) {
    assert(nmemb > 0);
    assert(return_ptr != NULL);

    xalloc_track_allocations_entry_volatile_t *entry =
            xalloc_track_allocations_allocate_per_thread_queue_get_next_free_slot_ptr_wait(
                    xalloc_track_allocations_per_thread_queue);

    entry->timestamp = intrinsics_tsc();
    entry->return_ptr = return_ptr;
    entry->caller = caller;

    entry->type = XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_CALLOC;
    entry->nmemb = nmemb;
    entry->size = size;

    xalloc_track_allocations_allocate_per_thread_queue_mark_enqueued(
            xalloc_track_allocations_per_thread_queue);
}

static void xalloc_track_allocations_realloc_wrapper(void *ptr, size_t size, void *return_ptr, void *caller) {
    assert(size > 0);
    assert(return_ptr != NULL);

    xalloc_track_allocations_entry_volatile_t *entry =
            xalloc_track_allocations_allocate_per_thread_queue_get_next_free_slot_ptr_wait(
                    xalloc_track_allocations_per_thread_queue);

    entry->timestamp = intrinsics_tsc();
    entry->return_ptr = return_ptr;
    entry->caller = caller;

    entry->type = XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_REALLOC;
    entry->ptr = ptr;
    entry->size = size;

    xalloc_track_allocations_allocate_per_thread_queue_mark_enqueued(
            xalloc_track_allocations_per_thread_queue);
}

static void xalloc_track_allocations_memalign_wrapper(size_t alignment, size_t size, void *return_ptr, void *caller) {
    assert(alignment > 0);
    assert(return_ptr != NULL);

    xalloc_track_allocations_entry_volatile_t *entry =
            xalloc_track_allocations_allocate_per_thread_queue_get_next_free_slot_ptr_wait(
                    xalloc_track_allocations_per_thread_queue);

    entry->timestamp = intrinsics_tsc();
    entry->return_ptr = return_ptr;
    entry->caller = caller;

    entry->type = XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MEMALIGN;
    entry->alignment = alignment;
    entry->size = size;

    xalloc_track_allocations_allocate_per_thread_queue_mark_enqueued(
            xalloc_track_allocations_per_thread_queue);
}

static void xalloc_track_allocations_free_wrapper(void* ptr, void *caller) {
    if (unlikely(!ptr)) {
        return;
    }

    xalloc_track_allocations_entry_volatile_t *entry =
            xalloc_track_allocations_allocate_per_thread_queue_get_next_free_slot_ptr_wait(
                    xalloc_track_allocations_per_thread_queue);

    entry->timestamp = intrinsics_tsc();
    entry->caller = caller;

    entry->type = XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_FREE;
    entry->ptr = ptr;
    entry->return_ptr = NULL;

    xalloc_track_allocations_allocate_per_thread_queue_mark_enqueued(
            xalloc_track_allocations_per_thread_queue);
}

static void xalloc_track_allocations_mmap_wrapper(void *addr, size_t length, int prot, int flags, int fd, off_t offset, void* return_ptr, void *caller) {
    if (unlikely(fd != -1)) {
        return;
    }

    xalloc_track_allocations_entry_volatile_t *entry =
            xalloc_track_allocations_allocate_per_thread_queue_get_next_free_slot_ptr_wait(
                    xalloc_track_allocations_per_thread_queue);

    entry->timestamp = intrinsics_tsc();
    entry->return_ptr = return_ptr;
    entry->caller = caller;

    entry->type = XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MMAP;
    entry->size = length;

    xalloc_track_allocations_allocate_per_thread_queue_mark_enqueued(
            xalloc_track_allocations_per_thread_queue);
}

static void xalloc_track_allocations_munmap_wrapper(void* ptr, size_t length, int return_ptr, void *caller) {
    if (unlikely(!ptr)) {
        return;
    }

    xalloc_track_allocations_entry_volatile_t *entry =
            xalloc_track_allocations_allocate_per_thread_queue_get_next_free_slot_ptr_wait(
                    xalloc_track_allocations_per_thread_queue);

    entry->timestamp = intrinsics_tsc();
    entry->caller = caller;

    entry->type = XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MUNMAP;
    entry->ptr = ptr;
    entry->size = length;

    xalloc_track_allocations_allocate_per_thread_queue_mark_enqueued(
            xalloc_track_allocations_per_thread_queue);
}

hashtable_spsc_t *xalloc_track_allocations_by_caller_hashtable = NULL;
hashtable_spsc_t *xalloc_track_allocations_by_pointer_hashtable = NULL;

struct xalloc_track_allocations_by_pointer_entry {
    void *caller;
    size_t size;
};
typedef struct xalloc_track_allocations_by_pointer_entry xalloc_track_allocations_by_pointer_entry_t;

static void xalloc_track_allocations_ctor_thread_func_loop() {
    static int processed_entries_max = 10;

    // Loop over the registered queues and process the available entries (max 10 per thread at a time)
    for(
            int registered_queue_index = 0;
            registered_queue_index < xalloc_track_allocations_registered_per_thread_queue_count;
            registered_queue_index++) {
        int processed_entries = 0;

        while (processed_entries < processed_entries_max) {
            processed_entries++;
            xalloc_track_allocations_per_thread_queue_volatile_t *queue =
                    xalloc_track_allocations_registered_per_thread_queue[registered_queue_index];

            bool found = false;
            uint64_t index = xalloc_track_allocations_allocate_per_thread_queue_get_next_full_slot_index(queue, &found);

            if (unlikely(!found)) {
                break;
            }

            xalloc_track_allocations_entry_volatile_t *entry =
                    xalloc_track_allocations_allocate_per_thread_queue_get_slot_ptr(queue, index);

            if (entry->type != XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MALLOC && entry->type != XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_CALLOC &&
                entry->type != XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_REALLOC && entry->type != XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MEMALIGN &&
                entry->type != XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_FREE && entry->type != XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MMAP &&
                entry->type != XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MUNMAP) {
                xalloc_track_allocations_allocate_per_thread_queue_mark_dequeued(queue);
                LOG_E(TAG, "xalloc track allocations: invalid entry type %d\n", entry->type);
                continue;
            }

            // Skip all the failed allocations
            if (entry->return_ptr == NULL && entry->type != XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_FREE
                && entry->type != XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MUNMAP) {
                xalloc_track_allocations_allocate_per_thread_queue_mark_dequeued(queue);
                continue;
            }

            // Calculate the size of the memory being allocated
            off_t pointer_size = 0;
            switch(entry->type) {
                case XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MEMALIGN:
                case XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MALLOC:
                case XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_REALLOC:
                case XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MMAP:
                    pointer_size = (off_t)entry->size;
                    break;
                case XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_CALLOC:
                    pointer_size = (off_t)(entry->size * entry->nmemb);
                    break;
            }

            // CALLER HANDLING

            // If the entry is a realloc or a free, we need to find the previous entry
            if (entry->type == XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_REALLOC ||
                entry->type == XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_FREE ||
                entry->type == XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MUNMAP) {
                if (entry->ptr != NULL) {
                    xalloc_track_allocations_by_pointer_entry_t *alloc_entry = hashtable_spsc_op_get_by_hash_and_key_uint64(
                            xalloc_track_allocations_by_pointer_hashtable,
                            (uint64_t) entry->ptr,
                            (uint64_t) entry->ptr);

                    // When doing a free or a realloc, the amount of the original caller has always to be reduced by
                    // the previously allocated amount
                    off_t amount_by_original_caller = (off_t) hashtable_spsc_op_get_by_hash_and_key_uint64(
                            xalloc_track_allocations_by_caller_hashtable,
                            (uint64_t) alloc_entry->caller,
                            (uint64_t) alloc_entry->caller);

                    amount_by_original_caller -= (off_t) alloc_entry->size;

                    // If the amount is zero, the entry can be removed otherwise needs to be updated
                    if (amount_by_original_caller <= 0) {
                        if (amount_by_original_caller < 0) {
                            LOG_W(TAG, "xalloc track allocations: amount by caller is negative");
                        }

                        if (!hashtable_spsc_op_delete_by_hash_and_key_uint64(
                                xalloc_track_allocations_by_caller_hashtable,
                                (uint64_t) alloc_entry->caller,
                                (uint64_t) alloc_entry->caller)) {
                            LOG_W(
                                    TAG,
                                    "xalloc track allocations: trying to delete non existing caller %p",
                                    alloc_entry->caller);
                        }
                    } else {
                        // Update the caller hashtable
                        if (!hashtable_spsc_op_try_set_by_hash_and_key_uint64(
                                xalloc_track_allocations_by_caller_hashtable,
                                (uint64_t) alloc_entry->caller,
                                (uint64_t) alloc_entry->caller,
                                (void*)amount_by_original_caller)) {
                            LOG_V(TAG, "xalloc track allocations: xalloc_track_allocations_by_caller_hashtable needs upsize");

                            // Try to upsize the hashtable
                            xalloc_track_allocations_by_caller_hashtable =
                                    hashtable_spsc_upsize(xalloc_track_allocations_by_caller_hashtable);

                            // Try to set the new amount again
                            if (!hashtable_spsc_op_try_set_by_hash_and_key_uint64(
                                    xalloc_track_allocations_by_caller_hashtable,
                                    (uint64_t) alloc_entry->caller,
                                    (uint64_t) alloc_entry->caller,
                                    (void*)amount_by_original_caller)) {
                                // Can't really fail now so if it happens there is an issue
                                FATAL(TAG, "xalloc track allocations: hashtable_spsc_op_try_set_by_hash_and_key_uint64 failed twice for the caller");
                            }
                        }
                    }
                }
            }

            if (entry->type != XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_FREE &&
                entry->type != XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MUNMAP) {
                off_t amount_by_new_caller = (off_t) hashtable_spsc_op_get_by_hash_and_key_uint64(
                        xalloc_track_allocations_by_caller_hashtable,
                        (uint64_t) entry->caller,
                        (uint64_t) entry->caller);

                amount_by_new_caller += pointer_size;

                // Update the caller hashtable
                if (!hashtable_spsc_op_try_set_by_hash_and_key_uint64(
                        xalloc_track_allocations_by_caller_hashtable,
                        (uint64_t) entry->caller,
                        (uint64_t) entry->caller,
                        (void*)amount_by_new_caller)) {
                    LOG_V(TAG, "xalloc track allocations: xalloc_track_allocations_by_caller_hashtable needs upsize");

                    // Try to upsize the hashtable
                    xalloc_track_allocations_by_caller_hashtable =
                            hashtable_spsc_upsize(xalloc_track_allocations_by_caller_hashtable);

                    // Try to set the new amount again
                    if (!hashtable_spsc_op_try_set_by_hash_and_key_uint64(
                            xalloc_track_allocations_by_caller_hashtable,
                            (uint64_t) entry->caller,
                            (uint64_t) entry->caller,
                            (void*)amount_by_new_caller)) {
                        // Can't really fail now so if it happens there is an issue
                        FATAL(TAG, "xalloc track allocations: hashtable_spsc_op_try_set_by_hash_and_key_uint64 failed twice for the caller");
                    }
                }
            }

            // POINTER HANDLING

            if ((entry->type == XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_REALLOC ||
                 entry->type == XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_FREE ||
                 entry->type == XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MUNMAP) &&
                 entry->ptr != NULL) {
                xalloc_track_allocations_by_pointer_entry_t *alloc_entry = hashtable_spsc_op_get_by_hash_and_key_uint64(
                        xalloc_track_allocations_by_pointer_hashtable,
                        (uint64_t) entry->ptr,
                        (uint64_t) entry->ptr);

                if (!alloc_entry) {
                    Dl_info dl_info;
                    if (dladdr(entry->caller, &dl_info) == 0) {
                        xalloc_track_allocations_allocate_per_thread_queue_mark_dequeued(queue);
                        LOG_E(TAG, "xalloc track allocations: dladdr failed");
                        continue;
                    }

                    LOG_W(
                            TAG,
                            "xalloc track allocations: trying to delete non tracked pointer %p [ %s / %s ]",
                            entry->ptr,
                            dl_info.dli_fname,
                            dl_info.dli_sname);
                } else {
                    if (!hashtable_spsc_op_delete_by_hash_and_key_uint64(
                            xalloc_track_allocations_by_pointer_hashtable,
                            (uint64_t) entry->ptr,
                            (uint64_t) entry->ptr)) {
                        Dl_info dl_info;
                        if (dladdr(entry->caller, &dl_info) == 0) {
                            xalloc_track_allocations_allocate_per_thread_queue_mark_dequeued(queue);
                            LOG_E(TAG, "xalloc track allocations: dladdr failed");
                            continue;
                        }

                        FATAL(
                                TAG,
                                "xalloc track allocations: failed to delete existing pointer %p [ %s / %s ]",
                                entry->ptr,
                                dl_info.dli_fname,
                                dl_info.dli_sname);
                    }
                }

                free(alloc_entry);
            }

            if (entry->type != XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_FREE && entry->type != XALLOC_TRACK_ALLOCATIONS_ENTRY_TYPE_MUNMAP) {
                xalloc_track_allocations_by_pointer_entry_t *new_alloc_entry =
                        (xalloc_track_allocations_by_pointer_entry_t*)malloc(
                                sizeof (xalloc_track_allocations_by_pointer_entry_t));
                new_alloc_entry->caller = entry->caller;
                new_alloc_entry->size = pointer_size;

                // Try to set the new amount
                if (!hashtable_spsc_op_try_set_by_hash_and_key_uint64(
                        xalloc_track_allocations_by_pointer_hashtable,
                        (uint64_t) entry->return_ptr,
                        (uint64_t) entry->return_ptr,
                        new_alloc_entry)) {
                    LOG_V(TAG, "xalloc track allocations: xalloc_track_allocations_by_pointer_hashtable needs upsize");

                    // Try to upsize the hashtable
                    xalloc_track_allocations_by_pointer_hashtable =
                            hashtable_spsc_upsize(xalloc_track_allocations_by_pointer_hashtable);

                    // Try to set the new amount again
                    if (!hashtable_spsc_op_try_set_by_hash_and_key_uint64(
                            xalloc_track_allocations_by_pointer_hashtable,
                            (uint64_t) entry->return_ptr,
                            (uint64_t) entry->return_ptr,
                            new_alloc_entry)) {
                        // Can't really fail now so if it happens there is an issue
                        FATAL(TAG,
                              "xalloc track allocations: hashtable_spsc_op_try_set_by_hash_and_key_uint64 failed twice for the pointer");
                    }
                }
            }

            xalloc_track_allocations_allocate_per_thread_queue_mark_dequeued(queue);
        }
    }
}

struct xalloc_track_allocations_info_from_pc_data {
    char *filename;
    int lineno;
    char *function;
    off_t size;
};
typedef struct xalloc_track_allocations_info_from_pc_data xalloc_track_allocations_info_from_pc_data_t;

int xalloc_track_allocations_get_info_from_pc_callback(
        void *data,
        uintptr_t pc,
        const char *filename,
        int lineno,
        const char *function) {

    xalloc_track_allocations_info_from_pc_data_t *info =
            (xalloc_track_allocations_info_from_pc_data_t*)data;
    info->filename = (char*)filename;
    info->lineno = lineno;
    info->function = (char*)function;

    return 0;
}

void xalloc_track_allocations_get_info_from_pc_callback_error(void *, const char *msg, int errnum) {
    fprintf(stderr, "Error %d occurred while trying to get the caller info: %s", errnum, msg);
    fflush(stderr);
}

int xalloc_track_allocations_info_from_pc_data_compare_desc(
        const void *a,
        const void *b) {

    if (((xalloc_track_allocations_info_from_pc_data_t*)a)->size < ((xalloc_track_allocations_info_from_pc_data_t*)b)->size) {
        return 1;
    } else if (((xalloc_track_allocations_info_from_pc_data_t*)a)->size > ((xalloc_track_allocations_info_from_pc_data_t*)b)->size) {
        return -1;
    } else {
        return 0;
    }
}

static void* xalloc_track_allocations_ctor_thread_func(void *user_data) {
    // To avoid the hooks being called from this thread, we set xalloc_track_allocations_in_hook to true to emulate
    // being in a hook, permanently
    xalloc_track_allocations_in_hook = true;

    // Set the thread status
    xalloc_track_allocations_ctor_thread_terminate = false;
    xalloc_track_allocations_ctor_thread_terminated = false;
    MEMORY_FENCE_STORE();

    // Acquire the path to the current executable
    char current_executable[1024];
    size_t current_executable_len = readlink("/proc/self/exe", current_executable, sizeof(current_executable));
    if (current_executable_len == -1) {
        FATAL(TAG, "xalloc track allocations: failed to readlink /proc/self/exe");
    }
    current_executable[current_executable_len] = '\0';

    // Acquire the path to the current executable
    __bt_state = backtrace_create_state(
            current_executable,
            0,
            xalloc_track_allocations_create_state_callback_error,
            NULL);

    if (pthread_setname_np(pthread_self(), "xalloctrkalloc") != 0) {
        LOG_W(TAG, "failed to set thread name");
    }

    xalloc_track_allocations_by_caller_hashtable = hashtable_spsc_new(
            32,
            128,
            false);
    xalloc_track_allocations_by_pointer_hashtable = hashtable_spsc_new(
            512,
            128,
            false);

    uint64_t start_time = clock_monotonic_int64_ms();

    while(!xalloc_track_allocations_ctor_thread_terminate) {
        xalloc_track_allocations_ctor_thread_func_loop();
        MEMORY_FENCE_LOAD();

        if (clock_monotonic_int64_ms() - start_time > 5 * 1000) {
            // Report the top 20 callers
            hashtable_spsc_bucket_t *buckets = hashtable_spsc_get_buckets(xalloc_track_allocations_by_caller_hashtable);

            fprintf(stdout, "Status report\n");

            {
                void *value;
                hashtable_spsc_bucket_index_t bucket_index = 0;

                // Count the max entries
                size_t max_entries_count = 0;
                while((value = hashtable_spsc_op_iter(xalloc_track_allocations_by_caller_hashtable, &bucket_index)) != NULL) {
                    max_entries_count++;
                    bucket_index++;
                }

                // Allocate enough memory to store n xalloc_track_allocations_info_from_pc_data_t entries
                xalloc_track_allocations_info_from_pc_data_t *entries = (xalloc_track_allocations_info_from_pc_data_t*)calloc(
                        max_entries_count,
                        sizeof(xalloc_track_allocations_info_from_pc_data_t));

                bucket_index = 0;
                size_t entry_index = 0;
                while((value = hashtable_spsc_op_iter(xalloc_track_allocations_by_caller_hashtable, &bucket_index)) != NULL) {
                    off_t amount_by_caller = (off_t) value;
                    void *caller = (void *) buckets[bucket_index].key_uint64;

                    xalloc_track_allocations_info_from_pc_data_t static_entry = { 0 };
                    backtrace_pcinfo(
                            __bt_state,
                            (uintptr_t) caller,
                            xalloc_track_allocations_get_info_from_pc_callback,
                            xalloc_track_allocations_get_info_from_pc_callback_error,
                            &static_entry);

                    if (static_entry.filename == NULL) {
                        bucket_index++;
                        continue;
                    } else if (strlen(static_entry.filename) < CACHEGRAND_BASE_PATH_LEN ||
                               strncmp(static_entry.filename, __FILE__, CACHEGRAND_BASE_PATH_LEN) != 0) {
                        bucket_index++;
                        continue;
                    }

                    xalloc_track_allocations_info_from_pc_data_t *entry = &entries[entry_index];
                    entry->lineno = static_entry.lineno;
                    entry->size = amount_by_caller;
                    entry->filename = (char *) malloc(strlen(static_entry.filename) + 1);
                    strncpy((char *) entry->filename, static_entry.filename, strlen(static_entry.filename) + 1);
                    entry->function = (char *) malloc(strlen(static_entry.function) + 1);
                    strncpy((char *) entry->function, static_entry.function, strlen(static_entry.function) + 1);

                    entry_index++;
                    bucket_index++;
                }

                size_t entries_count = entry_index;

                // Sort the entries
                qsort(
                        entries,
                        entries_count,
                        sizeof(xalloc_track_allocations_info_from_pc_data_t),
                        xalloc_track_allocations_info_from_pc_data_compare_desc);

                for (entry_index = 0; entry_index < entries_count; entry_index++) {
                    xalloc_track_allocations_info_from_pc_data_t *entry = &entries[entry_index];

                    char buffer_out[512] = { 0 };
                    sprintf(buffer_out, "[%s:%d / %s]", entry->filename + CACHEGRAND_BASE_PATH_LEN, entry->lineno, entry->function);

                    fprintf(stdout, "%s", buffer_out);
                    fprintf(
                            stdout,
                            "%.*s",
                            (int)(130 - strlen(buffer_out)),
                            "                                                                                                              ");

                    if (entry->size / 1024 / 1024 / 1024 > 0) {
                        fprintf(stdout, "%6.02f GB", (double)entry->size / 1024.0 / 1024.0 / 1024.0);
                    } else if (entry->size / 1024 / 1024 > 0) {
                        fprintf(stdout, "%6.02f MB", (double)entry->size / 1024.0 / 1024.0);
                    } else if (entry->size / 1024 > 0) {
                        fprintf(stdout, "%6.02f KB", (double)entry->size / 1024.0);
                    } else {
                        fprintf(stdout, "%6.02f B", (double)entry->size);
                    }

                    fprintf(stdout, "\n");
                }
            }

//            {
//                void *value;
//                hashtable_spsc_bucket_index_t bucket_index = 0;
//                int counter = 0;
//                while((value = hashtable_spsc_op_iter(xalloc_track_allocations_by_pointer_hashtable, &bucket_index)) != NULL) {
//                    counter++;
//                    xalloc_track_allocations_by_pointer_entry_t *entry = (xalloc_track_allocations_by_pointer_entry_t*)value;
//
//                    Dl_info dl_info;
//                    if (dladdr(entry->caller, &dl_info) == 0) {
//                        bucket_index++;
//                        continue;
//                    }
//
//                    if (dl_info.dli_fname == NULL || strncmp(dl_info.dli_fname, current_executable, current_executable_len) != 0) {
//                        bucket_index++;
//                        continue;
//                    }
//
//                    if (entry->size < 1024 * 1024) {
//                        bucket_index++;
//                        continue;
//                    }
//
//                    fprintf(stdout, "[%s] ", dl_info.dli_sname);
//
//                    if (entry->size / 1024 / 1024 / 1024 > 0) {
//                        fprintf(stdout, "%ld gb", entry->size / 1024 / 1024 / 1024);
//                    } else if (entry->size / 1024 / 1024 > 0) {
//                        fprintf(stdout, "%ld mb", entry->size / 1024 / 1024);
//                    } else if (entry->size / 1024 > 0) {
//                        fprintf(stdout, "%ld kb", entry->size / 1024);
//                    } else {
//                        fprintf(stdout, "%ld b", entry->size);
//                    }
//
//                    fprintf(stdout, "\n");
//
//                    bucket_index++;
//                }
//            }

            fprintf(stdout, "\n");
            fprintf(stdout, "\n");

            start_time = clock_monotonic_int64_ms();
        }
    }

    xalloc_track_allocations_ctor_thread_terminated = true;
    MEMORY_FENCE_STORE();

    fprintf(stdout, "xalloc track allocations background thread terminated (1)");
    fflush(stdout);


    void *value;
    hashtable_spsc_bucket_index_t bucket_index = 0;
    while((value = hashtable_spsc_op_iter(xalloc_track_allocations_by_pointer_hashtable, &bucket_index)) != NULL) {
        xalloc_free(value);
        bucket_index++;
    }

    hashtable_spsc_free(xalloc_track_allocations_by_caller_hashtable);
    hashtable_spsc_free(xalloc_track_allocations_by_pointer_hashtable);

    return NULL;
}

static void xalloc_track_allocations_ctor_start_thread() {
    pthread_attr_t thread_attr;

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

    LOG_I(TAG, "xalloc allocations tracking requested, starting background thread");

    if (pthread_create(
            &xalloc_track_allocations_thread,
            &thread_attr,
            xalloc_track_allocations_ctor_thread_func,
            NULL) != 0) {
        FATAL(TAG, "Failed to create xalloc track allocations thread");
    }

    pthread_attr_destroy(&thread_attr);

    LOG_I(TAG, "xalloc track allocations background thread started");
}

XALLOC_TRACK_ALLOCATIONS_HOOK_RETURN_PTR(void*, malloc, (size_t size), size);
XALLOC_TRACK_ALLOCATIONS_HOOK_RETURN_PTR(void*, calloc, (size_t nmemb, size_t size), nmemb, size);
XALLOC_TRACK_ALLOCATIONS_HOOK_RETURN_PTR(void*, realloc, (void *ptr, size_t size), ptr, size);
XALLOC_TRACK_ALLOCATIONS_HOOK_RETURN_PTR(void*, memalign, (size_t alignment, size_t size), alignment, size);
XALLOC_TRACK_ALLOCATIONS_HOOK_NO_RETURN(free, (void* ptr), ptr);
XALLOC_TRACK_ALLOCATIONS_HOOK_RETURN_PTR(void*, mmap, (void *addr, size_t length, int prot, int flags, int fd, off_t offset), addr, length, prot, flags, fd, offset);
XALLOC_TRACK_ALLOCATIONS_HOOK_RETURN_PTR(int, munmap, (void *ptr, size_t length), ptr, length);

FUNCTION_CTOR(xalloc_track_allocations_ctor, {
    xalloc_track_allocations_ctor_start_thread();
});

FUNCTION_DTOR(xalloc_track_allocations_dtor, {
    // Request the thread to terminate
    xalloc_track_allocations_ctor_thread_terminate = true;

    // Wait for the thread to terminate
    while(!xalloc_track_allocations_ctor_thread_terminated) {
        MEMORY_FENCE_LOAD();
    }

    fprintf(stdout, "xalloc track allocations background thread terminated (2)");
    fflush(stdout);
});
#endif
#endif
