/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

// In this special debug mode the memory allocators are not freed up in fast_fixed_memory_allocator_free but instead in
// the dtor invoked at the end of the execution to be able to access the metrics and other information.
// This might hide bugs and/or mess-up valgrind therefore it's controlled by a specific define that has to be set to 1
// inside fast_fixed_memory_allocator.c for safety reasons
#if FAST_FIXED_MEMORY_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>

#if __has_include(<valgrind/valgrind.h>)
#include <valgrind/valgrind.h>

#define HAS_VALGRIND
#endif

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "log/log.h"
#include "memory_fences.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "xalloc.h"
#include "fatal.h"
#include "hugepages.h"
#include "hugepage_cache.h"

#include "fast_fixed_memory_allocator.h"

#if FAST_FIXED_MEMORY_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif

#define TAG "fast_fixed_memory_allocator"

/**
 * The memory allocator requires hugepages, the hugepage address is 2MB aligned therefore it's possible to calculate the
 * initial address of the page from a pointer within and from there calculate the address to the metadata stored at the
 * beginning
 */

size_t fast_fixed_memory_allocator_os_page_size;
bool fast_fixed_memory_allocator_enabled = false;
static pthread_key_t fast_fixed_memory_allocator_thread_cache_key;

#if FAST_FIXED_MEMORY_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
// When in debug mode, allow the allocated memory allocators are tracked in a queue for later checks at shutdown
queue_mpmc_t *debug_fast_fixed_memory_allocator_list;
#endif

FUNCTION_CTOR(fast_fixed_memory_allocator_general_init, {
    fast_fixed_memory_allocator_os_page_size = xalloc_get_page_size();

    pthread_key_create(&fast_fixed_memory_allocator_thread_cache_key, fast_fixed_memory_allocator_thread_cache_free);

#if FAST_FIXED_MEMORY_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
    debug_fast_fixed_memory_allocator_list = queue_mpmc_init();
#endif
})

#if FAST_FIXED_MEMORY_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
void fast_fixed_memory_allocator_debug_allocs_frees_end() {
    fast_fixed_memory_allocator_t *fast_fixed_memory_allocator;
    fprintf(stdout, "> debug_fast_fixed_memory_allocator_list length <%d>\n", queue_mpmc_get_length(debug_fast_fixed_memory_allocator_list));
    fflush(stdout);

    fprintf(stdout, "+-%.10s-+-%.20s-+-%.14s-+-%.10s-+-%.11s-+-%.10s-+-%.12s-+-%.14s-+\n",
            "-------------------------------", "-------------------------------", "-------------------------------",
            "-------------------------------", "-------------------------------", "-------------------------------",
            "-------------------------------", "-------------------------------");

    fprintf(stdout, "| %-10s | %-20s | %-14s | %-10s | %-11s | %-10s | %-12s | %-14s |\n",
            "Thread Id", "Thread Name", "Allocator",
            "Obj Size", "Leaks Count", "Obj Count",
            "Slices Count", "Free queue Len");

    fprintf(stdout, "+-%.10s-+-%.20s-+-%.14s-+-%.10s-+-%.11s-+-%.10s-+-%.12s-+-%.14s-+\n",
            "-------------------------------", "-------------------------------", "-------------------------------",
            "-------------------------------", "-------------------------------", "-------------------------------",
            "-------------------------------", "-------------------------------");

    pid_t previous_thread_id = 0;
    while((fast_fixed_memory_allocator = (fast_fixed_memory_allocator_t*)queue_mpmc_pop(debug_fast_fixed_memory_allocator_list)) != NULL) {
        char thread_id_str[20] = { 0 };
        snprintf(thread_id_str, sizeof(thread_id_str) - 1, "%d", fast_fixed_memory_allocator->thread_id);

        uint32_t leaked_object_count =
                fast_fixed_memory_allocator->metrics.objects_inuse_count -
                        queue_mpmc_get_length(fast_fixed_memory_allocator->free_fast_fixed_memory_allocator_slots_queue_from_other_threads);

        if (leaked_object_count == 0) {
            continue;
        }

        fprintf(stdout, "| %-10s | %-20s | %p | %10u | %11u | %10u | %12u | %14u |\n",
                previous_thread_id == fast_fixed_memory_allocator->thread_id ? "" : thread_id_str,
                previous_thread_id == fast_fixed_memory_allocator->thread_id ? "" : fast_fixed_memory_allocator->thread_name,
                fast_fixed_memory_allocator,
                fast_fixed_memory_allocator->object_size,
                leaked_object_count,
                fast_fixed_memory_allocator->metrics.objects_inuse_count,
                fast_fixed_memory_allocator->metrics.slices_inuse_count,
                queue_mpmc_get_length(fast_fixed_memory_allocator->free_fast_fixed_memory_allocator_slots_queue_from_other_threads));
        fflush(stdout);

        queue_mpmc_free(fast_fixed_memory_allocator->free_fast_fixed_memory_allocator_slots_queue_from_other_threads);
        xalloc_free(fast_fixed_memory_allocator);

        previous_thread_id = fast_fixed_memory_allocator->thread_id;
    }

    fprintf(stdout, "+-%.10s-+-%.20s-+-%.14s-+-%.10s-+-%.11s-+-%.10s-+-%.12s-+-%.14s-+\n",
            "-------------------------------", "-------------------------------", "-------------------------------",
            "-------------------------------", "-------------------------------", "-------------------------------",
            "-------------------------------", "-------------------------------");
}
#endif

fast_fixed_memory_allocator_t **fast_fixed_memory_allocator_thread_cache_init() {
    fast_fixed_memory_allocator_t **thread_fast_fixed_memory_allocators = (fast_fixed_memory_allocator_t**)xalloc_alloc_zero(
            FAST_FIXED_MEMORY_ALLOCATOR_PREDEFINED_OBJECT_SIZES_COUNT * (sizeof(fast_fixed_memory_allocator_t*) + 1));

    for(int i = 0; i < FAST_FIXED_MEMORY_ALLOCATOR_PREDEFINED_OBJECT_SIZES_COUNT; i++) {
        uint32_t object_size = fast_fixed_memory_allocator_predefined_object_sizes[i];
        thread_fast_fixed_memory_allocators[fast_fixed_memory_allocator_index_by_object_size(object_size)] = fast_fixed_memory_allocator_init(object_size);
    }

    return thread_fast_fixed_memory_allocators;
}

void fast_fixed_memory_allocator_thread_cache_free(void *data) {
    fast_fixed_memory_allocator_t **thread_fast_fixed_memory_allocators = data;

    for(int i = 0; i < FAST_FIXED_MEMORY_ALLOCATOR_PREDEFINED_OBJECT_SIZES_COUNT; i++) {
        uint32_t object_size = fast_fixed_memory_allocator_predefined_object_sizes[i];
        uint32_t thread_fast_fixed_memory_allocators_index = fast_fixed_memory_allocator_index_by_object_size(object_size);
        fast_fixed_memory_allocator_free(thread_fast_fixed_memory_allocators[thread_fast_fixed_memory_allocators_index]);
        thread_fast_fixed_memory_allocators[thread_fast_fixed_memory_allocators_index] = NULL;
    }

    xalloc_free(data);
}

fast_fixed_memory_allocator_t** fast_fixed_memory_allocator_thread_cache_get() {
    fast_fixed_memory_allocator_t **thread_fast_fixed_memory_allocators = pthread_getspecific(fast_fixed_memory_allocator_thread_cache_key);
    return thread_fast_fixed_memory_allocators;
}

void fast_fixed_memory_allocator_thread_cache_set(
        fast_fixed_memory_allocator_t** fast_fixed_memory_allocators) {
    if (pthread_setspecific(fast_fixed_memory_allocator_thread_cache_key, fast_fixed_memory_allocators) != 0) {
        FATAL(TAG, "Unable to set the fast fixed memory allocator thread cache");
    }
}

bool fast_fixed_memory_allocator_thread_cache_has() {
    return fast_fixed_memory_allocator_thread_cache_get() != NULL;
}

void fast_fixed_memory_allocator_enable(
        bool enable) {
    if (unlikely(!fast_fixed_memory_allocator_thread_cache_has())) {
        fast_fixed_memory_allocator_thread_cache_set(fast_fixed_memory_allocator_thread_cache_init());
    }

    fast_fixed_memory_allocator_enabled = enable;
}

bool fast_fixed_memory_allocator_is_enabled() {
    return fast_fixed_memory_allocator_enabled;
}

uint8_t fast_fixed_memory_allocator_index_by_object_size(
        size_t object_size) {
    assert(object_size <= FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_MAX);

    if (object_size < FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_MIN) {
        object_size = FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_MIN;
    }

    // Round up the object_size to the next power of 2
    size_t rounded_up_object_size = object_size;
    rounded_up_object_size--;
    rounded_up_object_size |= rounded_up_object_size >> 1;
    rounded_up_object_size |= rounded_up_object_size >> 2;
    rounded_up_object_size |= rounded_up_object_size >> 4;
    rounded_up_object_size |= rounded_up_object_size >> 8;
    rounded_up_object_size |= rounded_up_object_size >> 16;
    rounded_up_object_size++;

    return 32 - __builtin_clz(rounded_up_object_size) - (32 - __builtin_clz(FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_MIN));
}

fast_fixed_memory_allocator_t* fast_fixed_memory_allocator_thread_cache_get_fast_fixed_memory_allocator_by_size(
        size_t object_size) {
    return fast_fixed_memory_allocator_thread_cache_get()[fast_fixed_memory_allocator_index_by_object_size(object_size)];
}

fast_fixed_memory_allocator_t* fast_fixed_memory_allocator_init(
        size_t object_size) {
    assert(object_size <= FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_MAX);

    fast_fixed_memory_allocator_t* fast_fixed_memory_allocator = (fast_fixed_memory_allocator_t*)xalloc_alloc_zero(sizeof(fast_fixed_memory_allocator_t));

    fast_fixed_memory_allocator->object_size = object_size;
    fast_fixed_memory_allocator->slots = double_linked_list_init();
    fast_fixed_memory_allocator->slices = double_linked_list_init();
    fast_fixed_memory_allocator->metrics.slices_inuse_count = 0;
    fast_fixed_memory_allocator->metrics.objects_inuse_count = 0;
    fast_fixed_memory_allocator->free_fast_fixed_memory_allocator_slots_queue_from_other_threads = queue_mpmc_init();

#if FAST_FIXED_MEMORY_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
    fast_fixed_memory_allocator->thread_id = syscall(SYS_gettid);
    pthread_getname_np(pthread_self(), fast_fixed_memory_allocator->thread_name, sizeof(fast_fixed_memory_allocator->thread_name) - 1);

    queue_mpmc_push(debug_fast_fixed_memory_allocator_list, fast_fixed_memory_allocator);
#endif

    return fast_fixed_memory_allocator;
}

bool fast_fixed_memory_allocator_free(
        fast_fixed_memory_allocator_t* fast_fixed_memory_allocator) {
    double_linked_list_item_t* item;
    fast_fixed_memory_allocator_slot_t *fast_fixed_memory_allocator_slot;

    fast_fixed_memory_allocator->fast_fixed_memory_allocator_freed = true;
    MEMORY_FENCE_STORE();

    // If there are objects in use they are most likely owned in use in some other threads and therefore the memory
    // can't be freed. The ownership of the operation fall upon the thread that will return the last object.
    // Not optimal, as it would be better to use a dying thread to free up memory instead of an in-use thread.
    // For currently cachegrand doesn't matter really because the threads are long-lived and only terminate when it
    // shuts-down.
    uint32_t objects_inuse_count =
            fast_fixed_memory_allocator->metrics.objects_inuse_count -
            queue_mpmc_get_length(fast_fixed_memory_allocator->free_fast_fixed_memory_allocator_slots_queue_from_other_threads);
    if (objects_inuse_count > 0) {
        return false;
    }

    // Clean up the free list
    while((fast_fixed_memory_allocator_slot = queue_mpmc_pop(fast_fixed_memory_allocator->free_fast_fixed_memory_allocator_slots_queue_from_other_threads)) != NULL) {
        fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice = fast_fixed_memory_allocator_slice_from_memptr(fast_fixed_memory_allocator_slot->data.memptr);
        fast_fixed_memory_allocator_mem_free_hugepages_current_thread(fast_fixed_memory_allocator, fast_fixed_memory_allocator_slice, fast_fixed_memory_allocator_slot);
    }

    // Can't iterate using the normal double_linked_list_iter_next as the double_linked_list_item is embedded in the
    // hugepage and the hugepage is going to get freed
    item = fast_fixed_memory_allocator->slices->head;
    while(item != NULL) {
        fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice = item->data;
        item = item->next;
        hugepage_cache_push(fast_fixed_memory_allocator_slice->data.page_addr);
    }

    double_linked_list_free(fast_fixed_memory_allocator->slices);
    double_linked_list_free(fast_fixed_memory_allocator->slots);

#if FAST_FIXED_MEMORY_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
    // Do nothing really, it's to ensure that the memory will get always freed if the condition checked is not 1
#else
    queue_mpmc_free(fast_fixed_memory_allocator->free_fast_fixed_memory_allocator_slots_queue_from_other_threads);
    xalloc_free(fast_fixed_memory_allocator);
#endif

    return true;
}

size_t fast_fixed_memory_allocator_slice_calculate_usable_hugepage_size() {
    size_t hugepage_size = HUGEPAGE_SIZE_2MB;
    size_t fast_fixed_memory_allocator_slice_size = sizeof(fast_fixed_memory_allocator_slice_t);
    size_t usable_hugepage_size = hugepage_size - fast_fixed_memory_allocator_os_page_size - fast_fixed_memory_allocator_slice_size;

    return usable_hugepage_size;
}

uint32_t fast_fixed_memory_allocator_slice_calculate_data_offset(
        size_t usable_hugepage_size,
        size_t object_size) {
    uint32_t slots_count = (int)(usable_hugepage_size / (object_size + sizeof(fast_fixed_memory_allocator_slot_t)));
    size_t data_offset = sizeof(fast_fixed_memory_allocator_slice_t) + (slots_count * sizeof(fast_fixed_memory_allocator_slot_t));
    data_offset += fast_fixed_memory_allocator_os_page_size - (data_offset % fast_fixed_memory_allocator_os_page_size);

    return data_offset;
}

uint32_t fast_fixed_memory_allocator_slice_calculate_slots_count(
        size_t usable_hugepage_size,
        size_t data_offset,
        size_t object_size) {
    size_t data_size = usable_hugepage_size - data_offset;
    uint32_t slots_count = data_size / object_size;

    return slots_count;
}

fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice_init(
        fast_fixed_memory_allocator_t* fast_fixed_memory_allocator,
        void* memptr) {
    fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice = (fast_fixed_memory_allocator_slice_t*)memptr;

    size_t usable_hugepage_size = fast_fixed_memory_allocator_slice_calculate_usable_hugepage_size();
    uint32_t data_offset = fast_fixed_memory_allocator_slice_calculate_data_offset(
            usable_hugepage_size,
            fast_fixed_memory_allocator->object_size);
    uint32_t slots_count = fast_fixed_memory_allocator_slice_calculate_slots_count(
            usable_hugepage_size,
            data_offset,
            fast_fixed_memory_allocator->object_size);

    fast_fixed_memory_allocator_slice->data.fast_fixed_memory_allocator = fast_fixed_memory_allocator;
    fast_fixed_memory_allocator_slice->data.page_addr = memptr;
    fast_fixed_memory_allocator_slice->data.data_addr = (uintptr_t)memptr + data_offset;
    fast_fixed_memory_allocator_slice->data.metrics.objects_total_count = slots_count;
    fast_fixed_memory_allocator_slice->data.metrics.objects_inuse_count = 0;
    fast_fixed_memory_allocator_slice->data.available = true;

    return fast_fixed_memory_allocator_slice;
}

void fast_fixed_memory_allocator_slice_add_slots_to_per_thread_metadata_slots(
        fast_fixed_memory_allocator_t* fast_fixed_memory_allocator,
        fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice) {
    fast_fixed_memory_allocator_slot_t* fast_fixed_memory_allocator_slot;
    for(uint32_t index = 0; index < fast_fixed_memory_allocator_slice->data.metrics.objects_total_count; index++) {
        fast_fixed_memory_allocator_slot = &fast_fixed_memory_allocator_slice->data.slots[index];
        fast_fixed_memory_allocator_slot->data.available = true;
        fast_fixed_memory_allocator_slot->data.memptr = (void*)(fast_fixed_memory_allocator_slice->data.data_addr + (index * fast_fixed_memory_allocator->object_size));

#if DEBUG == 1
        fast_fixed_memory_allocator_slot->data.allocs = 0;
        fast_fixed_memory_allocator_slot->data.frees = 0;
#endif

        double_linked_list_unshift_item(
                fast_fixed_memory_allocator->slots,
                &fast_fixed_memory_allocator_slot->double_linked_list_item);
    }
}

void fast_fixed_memory_allocator_slice_remove_slots_from_per_thread_metadata_slots(
        fast_fixed_memory_allocator_t* fast_fixed_memory_allocator,
        fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice) {
    fast_fixed_memory_allocator_slot_t* fast_fixed_memory_allocator_slot;
    for(uint32_t index = 0; index < fast_fixed_memory_allocator_slice->data.metrics.objects_total_count; index++) {
        fast_fixed_memory_allocator_slot = &fast_fixed_memory_allocator_slice->data.slots[index];
        double_linked_list_remove_item(
                fast_fixed_memory_allocator->slots,
                &fast_fixed_memory_allocator_slot->double_linked_list_item);
    }
}

fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice_from_memptr(
        void* memptr) {
    fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice = memptr - ((uintptr_t)memptr % HUGEPAGE_SIZE_2MB);
    return fast_fixed_memory_allocator_slice;
}

void fast_fixed_memory_allocator_slice_make_available(
        fast_fixed_memory_allocator_t* fast_fixed_memory_allocator,
        fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice) {
    fast_fixed_memory_allocator_slice_remove_slots_from_per_thread_metadata_slots(
            fast_fixed_memory_allocator,
            fast_fixed_memory_allocator_slice);

    fast_fixed_memory_allocator->metrics.slices_inuse_count--;

    double_linked_list_remove_item(
            fast_fixed_memory_allocator->slices,
            &fast_fixed_memory_allocator_slice->double_linked_list_item);
}

fast_fixed_memory_allocator_slot_t* fast_fixed_memory_allocator_slot_from_memptr(
        fast_fixed_memory_allocator_t* fast_fixed_memory_allocator,
        fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice,
        void* memptr) {
    uint16_t object_index = ((uintptr_t)memptr - (uintptr_t)fast_fixed_memory_allocator_slice->data.data_addr) / fast_fixed_memory_allocator->object_size;
    fast_fixed_memory_allocator_slot_t* slot = &fast_fixed_memory_allocator_slice->data.slots[object_index];

    return slot;
}

void fast_fixed_memory_allocator_grow(
        fast_fixed_memory_allocator_t* fast_fixed_memory_allocator,
        void* memptr) {
    // Initialize the new slice and set it to non-available because it's going to be immediately used
    fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice = fast_fixed_memory_allocator_slice_init(
            fast_fixed_memory_allocator,
            memptr);
    fast_fixed_memory_allocator_slice->data.available = false;

    // Add all the slots to the double linked list
    fast_fixed_memory_allocator_slice_add_slots_to_per_thread_metadata_slots(
            fast_fixed_memory_allocator,
            fast_fixed_memory_allocator_slice);
    fast_fixed_memory_allocator->metrics.slices_inuse_count++;

    double_linked_list_push_item(
            fast_fixed_memory_allocator->slices,
            &fast_fixed_memory_allocator_slice->double_linked_list_item);
}

void* fast_fixed_memory_allocator_mem_alloc_hugepages(
        fast_fixed_memory_allocator_t* fast_fixed_memory_allocator,
        size_t size) {
    assert(size <= FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_MAX);

    double_linked_list_t* slots_list;
    double_linked_list_item_t* slots_head_item;
    fast_fixed_memory_allocator_slot_t* fast_fixed_memory_allocator_slot = NULL;
    fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice = NULL;

    // Always tries first to get a slow from the local cache, it's faster
    slots_list = fast_fixed_memory_allocator->slots;
    slots_head_item = slots_list->head;
    fast_fixed_memory_allocator_slot = (fast_fixed_memory_allocator_slot_t*)slots_head_item;

    // If it can't get the slot from the local cache tries to fetch if from the free list which is a bit slower as it
    // involves atomic operations, on the other end it requires less operation to be prepared as e.g. it is already on
    // the correct side of the slots double linked list
    if (
            (fast_fixed_memory_allocator_slot == NULL || fast_fixed_memory_allocator_slot->data.available == false) &&
            (fast_fixed_memory_allocator_slot = queue_mpmc_pop(fast_fixed_memory_allocator->free_fast_fixed_memory_allocator_slots_queue_from_other_threads)) != NULL) {
        assert(fast_fixed_memory_allocator_slot->data.memptr != NULL);
        fast_fixed_memory_allocator_slot->data.available = false;

#if DEBUG == 1
        fast_fixed_memory_allocator_slot->data.allocs++;

#if defined(HAS_VALGRIND)
        fast_fixed_memory_allocator_slice = fast_fixed_memory_allocator_slice_from_memptr(fast_fixed_memory_allocator_slot->data.memptr);
        VALGRIND_MEMPOOL_ALLOC(fast_fixed_memory_allocator_slice->data.page_addr, fast_fixed_memory_allocator_slot->data.memptr, size);
#endif
#endif

        // To keep the code and avoid convoluted ifs, the code returns here
        return fast_fixed_memory_allocator_slot->data.memptr;
    }

    if (fast_fixed_memory_allocator_slot == NULL || fast_fixed_memory_allocator_slot->data.available == false) {
        void* hugepage_addr = hugepage_cache_pop();

#if DEBUG == 1
#if defined(HAS_VALGRIND)
        VALGRIND_CREATE_MEMPOOL(hugepage_addr, 0, false);
#endif
#endif

        if (!hugepage_addr) {
            LOG_E(TAG, "Unable to allocate %lu bytes of memory, no hugepages available", size);
            return NULL;
        }

        fast_fixed_memory_allocator_grow(
                fast_fixed_memory_allocator,
                hugepage_addr);

        slots_head_item = slots_list->head;
        fast_fixed_memory_allocator_slot = (fast_fixed_memory_allocator_slot_t*)slots_head_item;
    }

    assert(fast_fixed_memory_allocator_slot->data.memptr != NULL);
    assert(fast_fixed_memory_allocator_slot->data.allocs == fast_fixed_memory_allocator_slot->data.frees);

    double_linked_list_move_item_to_tail(slots_list, slots_head_item);

    fast_fixed_memory_allocator_slice = fast_fixed_memory_allocator_slice_from_memptr(fast_fixed_memory_allocator_slot->data.memptr);
    fast_fixed_memory_allocator_slice->data.metrics.objects_inuse_count++;
    fast_fixed_memory_allocator->metrics.objects_inuse_count++;

    fast_fixed_memory_allocator_slot->data.available = false;
#if DEBUG == 1
    fast_fixed_memory_allocator_slot->data.allocs++;

#if defined(HAS_VALGRIND)
    VALGRIND_MEMPOOL_ALLOC(fast_fixed_memory_allocator_slice->data.page_addr, fast_fixed_memory_allocator_slot->data.memptr, size);
#endif
#endif

    MEMORY_FENCE_STORE();

    return fast_fixed_memory_allocator_slot->data.memptr;
}

void* fast_fixed_memory_allocator_mem_alloc_zero(
        size_t size) {
    void* memptr = fast_fixed_memory_allocator_mem_alloc(size);
    if (memptr) {
        memset(memptr, 0, size);
    }

    return memptr;
}

void fast_fixed_memory_allocator_mem_free_hugepages_current_thread(
        fast_fixed_memory_allocator_t* fast_fixed_memory_allocator,
        fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice,
        fast_fixed_memory_allocator_slot_t* fast_fixed_memory_allocator_slot) {
    bool can_free_fast_fixed_memory_allocator_slice = false;
    // Update the availability and the metrics
    fast_fixed_memory_allocator_slice->data.metrics.objects_inuse_count--;
    fast_fixed_memory_allocator->metrics.objects_inuse_count--;
    fast_fixed_memory_allocator_slot->data.available = true;

    // Move the slot back to the head because it's available
    double_linked_list_move_item_to_head(
            fast_fixed_memory_allocator->slots,
            &fast_fixed_memory_allocator_slot->double_linked_list_item);

    // If the slice is empty and for the currently core there is already another empty slice, make the current
    // slice available for other cores in the same numa node
    if (fast_fixed_memory_allocator_slice->data.metrics.objects_inuse_count == 0) {
        fast_fixed_memory_allocator_slice_make_available(fast_fixed_memory_allocator, fast_fixed_memory_allocator_slice);
        can_free_fast_fixed_memory_allocator_slice = true;
    }

    if (can_free_fast_fixed_memory_allocator_slice) {
#if DEBUG == 1
#if defined(HAS_VALGRIND)
        VALGRIND_DESTROY_MEMPOOL(fast_fixed_memory_allocator_slice->data.page_addr);
#endif
#endif
        hugepage_cache_push(fast_fixed_memory_allocator_slice->data.page_addr);
    }

    MEMORY_FENCE_STORE();
}

void fast_fixed_memory_allocator_mem_free_hugepages_different_thread(
        fast_fixed_memory_allocator_t* fast_fixed_memory_allocator,
        fast_fixed_memory_allocator_slot_t* fast_fixed_memory_allocator_slot) {
    if (unlikely(!queue_mpmc_push(fast_fixed_memory_allocator->free_fast_fixed_memory_allocator_slots_queue_from_other_threads, fast_fixed_memory_allocator_slot))) {
        FATAL(TAG, "Can't pass the slot to free to the owning thread, unable to continue");
    }

    // To determine which thread can clean up the data the code simply checks if objects_inuse_count - length of the
    // free_fast_fixed_memory_allocator_slots_queue queue is equals to 0, if it is this thread can perform the final clean up.
    // The objects_inuse_count is not atomic but memory fences are in use
    MEMORY_FENCE_LOAD();
    if (unlikely(fast_fixed_memory_allocator->fast_fixed_memory_allocator_freed)) {
        // If the last object pushed was the last that needed to be freed, fast_fixed_memory_allocator_free can be invoked.
        bool can_free_fast_fixed_memory_allocator =
                (fast_fixed_memory_allocator->metrics.objects_inuse_count -
                queue_mpmc_get_length(fast_fixed_memory_allocator->free_fast_fixed_memory_allocator_slots_queue_from_other_threads)) == 0;
        if (unlikely(can_free_fast_fixed_memory_allocator)) {
            fast_fixed_memory_allocator_free(fast_fixed_memory_allocator);
        }
    }
}

void fast_fixed_memory_allocator_mem_free_hugepages(
        void* memptr) {
    // Acquire the fast_fixed_memory_allocator_slice, the fast_fixed_memory_allocator, the fast_fixed_memory_allocator_slot and the thread_metadata related to the memory to be
    // freed. The slice holds a pointer to the memory allocator so the slot will always be put back into the correct thread
    fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice =
            fast_fixed_memory_allocator_slice_from_memptr(memptr);
    fast_fixed_memory_allocator_t* fast_fixed_memory_allocator =
            fast_fixed_memory_allocator_slice->data.fast_fixed_memory_allocator;
    fast_fixed_memory_allocator_slot_t* fast_fixed_memory_allocator_slot =
            fast_fixed_memory_allocator_slot_from_memptr(
                    fast_fixed_memory_allocator,
                    fast_fixed_memory_allocator_slice,
                    memptr);

    // Test to catch double free
    assert(fast_fixed_memory_allocator_slot->data.available == false);

#if DEBUG == 1
    fast_fixed_memory_allocator_slot->data.frees++;

#if defined(HAS_VALGRIND)
    VALGRIND_MEMPOOL_FREE(fast_fixed_memory_allocator_slice->data.page_addr, fast_fixed_memory_allocator_slot->data.memptr);
#endif
#endif

    // Check if the memory is owned by a different thread, the memory can't be freed directly but has to be passed to
    // the thread owning it. In cachegrand most of the allocations are freed by the owning thread.
    bool is_different_thread = fast_fixed_memory_allocator != fast_fixed_memory_allocator_thread_cache_get_fast_fixed_memory_allocator_by_size(
            fast_fixed_memory_allocator->object_size);
    if (unlikely(is_different_thread)) {
        // This is slow path as it involves always atomic ops and potentially also a spinlock
        fast_fixed_memory_allocator_mem_free_hugepages_different_thread(fast_fixed_memory_allocator, fast_fixed_memory_allocator_slot);
    } else {
        fast_fixed_memory_allocator_mem_free_hugepages_current_thread(fast_fixed_memory_allocator, fast_fixed_memory_allocator_slice, fast_fixed_memory_allocator_slot);
    }
}

void* fast_fixed_memory_allocator_mem_alloc_xalloc(
        size_t size) {
    return xalloc_alloc(size);
}

void fast_fixed_memory_allocator_mem_free_xalloc(
        void* memptr) {
    xalloc_free(memptr);
}

void* fast_fixed_memory_allocator_mem_alloc(
        size_t size) {
    void* memptr;

    assert(size > 0);

    if (likely(fast_fixed_memory_allocator_enabled)) {
        if (unlikely(!fast_fixed_memory_allocator_thread_cache_has())) {
            fast_fixed_memory_allocator_thread_cache_set(fast_fixed_memory_allocator_thread_cache_init());
        }

        fast_fixed_memory_allocator_t *fast_fixed_memory_allocator = fast_fixed_memory_allocator_thread_cache_get()[fast_fixed_memory_allocator_index_by_object_size(size)];
        memptr = fast_fixed_memory_allocator_mem_alloc_hugepages(fast_fixed_memory_allocator, size);
    } else {
        memptr = fast_fixed_memory_allocator_mem_alloc_xalloc(size);
    }

    return memptr;
}

void* fast_fixed_memory_allocator_mem_realloc(
        void* memptr,
        size_t current_size,
        size_t new_size,
        bool zero_new_memory) {
    // TODO: the implementation is terrible, it's not even checking if the new size fits within the provided slot
    //       because in case a new allocation is not really needed
    void* new_memptr;

    new_memptr = fast_fixed_memory_allocator_mem_alloc(new_size);

    // If the new allocation doesn't fail check if it has to be zeroed
    if (!new_memptr) {
        return new_memptr;
    }

    // Always free the pointer passed, even if the realloc fails
    if (memptr != NULL) {
        memcpy(new_memptr, memptr, current_size);
        fast_fixed_memory_allocator_mem_free(memptr);
    }

    if (zero_new_memory) {
        memset(new_memptr + current_size, 0, new_size - current_size);
    }

    return new_memptr;
}

void fast_fixed_memory_allocator_mem_free(
        void* memptr) {
    if (likely(fast_fixed_memory_allocator_enabled)) {
        if (unlikely(!fast_fixed_memory_allocator_thread_cache_has())) {
            fast_fixed_memory_allocator_thread_cache_set(fast_fixed_memory_allocator_thread_cache_init());
        }

        fast_fixed_memory_allocator_mem_free_hugepages(memptr);
    } else {
        fast_fixed_memory_allocator_mem_free_xalloc(memptr);
    }
}
