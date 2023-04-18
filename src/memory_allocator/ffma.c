/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

// In this special debug mode the memory allocators are not freed up in ffma_free but instead in
// the dtor invoked at the end of the execution to be able to access the metrics and other information.
// This might hide bugs and/or mess-up valgrind therefore it's controlled by a specific define that has to be set to 1
// inside ffma.c for safety reasons
#if FFMA_DEBUG_ALLOCS_FREES == 1
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdatomic.h>

#include "misc.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "xalloc.h"
#include "fatal.h"
#include "ffma_region_cache.h"

#include "ffma.h"
#include "ffma_thread_cache.h"

#if FFMA_DEBUG_ALLOCS_FREES == 1
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif

#define TAG FFMA_LOG_TAG_INTERNAL

/**
 * The memory allocator requires regions aligned to size of the region itself to calculate the initial address of the
 * page from a pointer within and from there calculate the address to the metadata stored at the beginning.
 *
 * On Linux it's possible to use the MAP_FIXED_NOREPLACE flag to force the allocation to be aligned to the requested
 * size or fail if it's not possible or the address is already in use.
 *
 * When using mmap the ffma_region_cache will try to find a random address aligned to the region and then allocate it.
 * When mmap fails because an address is already in use, it will try to generate a new random address and try again.
 */

static size_t ffma_os_page_size;
ffma_region_cache_t *internal_ffma_region_cache;

#if FFMA_DEBUG_ALLOCS_FREES == 1
// When in debug mode, allow the allocated memory allocators are tracked in a queue for later checks at shutdown
queue_mpmc_t *debug_ffma_list;
#endif

FUNCTION_CTOR(ffma_init_ctor, {
    // Get the OS page size
    ffma_os_page_size = xalloc_get_page_size();

    // Allocates the internal region cache
    internal_ffma_region_cache = ffma_region_cache_init(
            FFMA_SLICE_SIZE,
            FFMA_REGION_CACHE_SIZE,
            false);

#if FFMA_DEBUG_ALLOCS_FREES == 1
    // If the debug mode for the allocators is enabled then initialize the list of ffma allocators to track
    debug_ffma_list = queue_mpmc_init();
#endif
})

FUNCTION_DTOR(ffma_init_dtor, {
    ffma_region_cache_free(internal_ffma_region_cache);
})

#if FFMA_DEBUG_ALLOCS_FREES == 1
void ffma_debug_allocs_frees_end() {
    ffma_t *ffma;
    fprintf(stdout, "> debug_ffma_list length <%d>\n", queue_mpmc_get_length(debug_ffma_list));
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

    bool header_printed = false;
    pid_t previous_thread_id = 0;
    while((ffma = (ffma_t*)queue_mpmc_pop(debug_ffma_list)) != NULL) {
        char thread_id_str[20] = { 0 };
        snprintf(thread_id_str, sizeof(thread_id_str) - 1, "%d", ffma->thread_id);

        uint32_t leaked_object_count =
                ffma->metrics.objects_inuse_count -
                        queue_mpmc_get_length(&ffma->free_ffma_slots_queue_from_other_threads);

        if (leaked_object_count == 0) {
            continue;
        }

        // Flush the queue of slots from other threads to avoid polluting the object, these slots are marked as occupied
        // as they have been freed by a thread other than the one that allocated them but they didn't get reused.
        // As this function is invoked ONLY at the very end of the program, it's safe to assume that the slots in the
        // queue are not going to be reused and can be processed as if they were freed.
        ffma_flush_slots_queue_from_other_threads(ffma);

        if (header_printed == false) {
            ffma_debug_allocs_frees_end_print_header();
            header_printed = true;
        }

        fprintf(stdout, "| %-10s | %-20s | %p | %10u | %11u | %10u | %12u | %14u |\n",
                previous_thread_id == ffma->thread_id ? "" : thread_id_str,
                previous_thread_id == ffma->thread_id ? "" : ffma->thread_name,
                ffma,
                ffma->object_size,
                leaked_object_count,
                ffma->metrics.objects_inuse_count,
                ffma->metrics.slices_inuse_count,
                queue_mpmc_get_length(ffma->free_ffma_slots_queue_from_other_threads));
        fflush(stdout);

        queue_mpmc_free(ffma->free_ffma_slots_queue_from_other_threads);
        xalloc_free(ffma);

        previous_thread_id = ffma->thread_id;
    }

    fprintf(stdout, "+-%.10s-+-%.20s-+-%.14s-+-%.10s-+-%.11s-+-%.10s-+-%.12s-+-%.14s-+\n",
            "-------------------------------", "-------------------------------", "-------------------------------",
            "-------------------------------", "-------------------------------", "-------------------------------",
            "-------------------------------", "-------------------------------");
}
#endif

void ffma_set_use_hugepages(
        bool use_hugepages) {
    internal_ffma_region_cache->use_hugepages = use_hugepages;
}

ffma_t* ffma_init(
        size_t object_size) {
    assert(object_size <= FFMA_OBJECT_SIZE_MAX);

    ffma_t* ffma = (ffma_t*)xalloc_mmap_alloc(sizeof(ffma_t));

    ffma->object_size = object_size;
    ffma->slots = double_linked_list_init();
    ffma->slices = double_linked_list_init();
    ffma->metrics.slices_inuse_count = 0;
    ffma->metrics.objects_inuse_count = 0;
    queue_mpmc_init_noalloc(&ffma->free_ffma_slots_queue_from_other_threads);

#if FFMA_DEBUG_ALLOCS_FREES == 1
    ffma->thread_id = syscall(SYS_gettid);
    pthread_getname_np(pthread_self(), ffma->thread_name, sizeof(ffma->thread_name) - 1);

    queue_mpmc_push(debug_ffma_list, ffma);
#endif

    return ffma;
}

void ffma_flush_slots_queue_from_other_threads(
    ffma_t* ffma) {
    ffma_slot_t *ffma_slot;

    // Clean up the free list
    while((ffma_slot = queue_mpmc_pop(&ffma->free_ffma_slots_queue_from_other_threads)) != NULL) {
        ffma_slice_t* ffma_slice = ffma_slice_from_memptr(ffma_slot->data.memptr);
        ffma_mem_free_slot_in_current_thread(ffma, ffma_slice, ffma_slot);
    }
}

void ffma_flush(
        ffma_t* ffma) {
    double_linked_list_item_t* item;

    if (ffma->metrics.objects_inuse_count == 0) {
        return;
    }

    // Clean up the free list
    ffma_flush_slots_queue_from_other_threads(ffma);

    // Can't iterate using the normal double_linked_list_iter_next as the double_linked_list_item is embedded in the
    // allocated memory and the memory is going to get freed
    item = ffma->slices->head;
    while(item != NULL) {
        ffma_slice_t* ffma_slice = (ffma_slice_t *)item;
        item = item->next;

#if defined(HAS_VALGRIND)
        VALGRIND_DESTROY_MEMPOOL(ffma_slice->data.page_addr);
#endif
        ffma_region_cache_push(internal_ffma_region_cache, ffma_slice->data.page_addr);
    }

    queue_mpmc_free_noalloc(&ffma->free_ffma_slots_queue_from_other_threads);
}

void ffma_free(
        ffma_t* ffma) {
    double_linked_list_free(ffma->slices);
    double_linked_list_free(ffma->slots);
    xalloc_mmap_free(ffma, sizeof(ffma_t));
}

ffma_slice_t* ffma_slice_init(
        ffma_t* ffma,
        void* memptr) {
    ffma_slice_t* ffma_slice = (ffma_slice_t*)memptr;

    size_t usable_memory_size = ffma_slice_calculate_usable_memory_size(ffma_os_page_size);
    uint32_t data_offset = ffma_slice_calculate_data_offset(
            ffma_os_page_size,
            usable_memory_size,
            ffma->object_size);
    uint32_t slots_count = ffma_slice_calculate_slots_count(
            usable_memory_size,
            data_offset,
            ffma->object_size);

    ffma_slice->data.ffma = ffma;
    ffma_slice->data.page_addr = memptr;
    ffma_slice->data.data_addr = (uintptr_t)memptr + data_offset;
    ffma_slice->data.metrics.objects_total_count = slots_count;
    ffma_slice->data.metrics.objects_inuse_count = 0;
    ffma_slice->data.available = true;
    ffma_slice->data.metrics.objects_total_count = slots_count;
    ffma_slice->data.metrics.objects_initialized_count = 0;

    return ffma_slice;
}

void ffma_slice_add_slots_to_per_thread_metadata_slots(
        ffma_t* ffma,
        ffma_slice_t* ffma_slice) {
    ffma_slot_t* ffma_slot;
    for(uint32_t index = 0; index < ffma_slice->data.metrics.objects_total_count; index++) {
        ffma_slot = &ffma_slice->data.slots[index];
        ffma_slot->data.available = true;
        ffma_slot->data.memptr = (void*)(ffma_slice->data.data_addr + (index * ffma->object_size));

#if DEBUG == 1
        ffma_slot->data.allocs = 0;
        ffma_slot->data.frees = 0;
#endif

        double_linked_list_unshift_item(
                ffma->slots,
                &ffma_slot->double_linked_list_item);

        ffma_slice->data.metrics.objects_initialized_count++;
    }
}

void ffma_slice_remove_slots_from_per_thread_metadata_slots(
        ffma_t* ffma,
        ffma_slice_t* ffma_slice) {
    ffma_slot_t* ffma_slot;
    for(uint32_t index = 0; index < ffma_slice->data.metrics.objects_total_count; index++) {
        ffma_slot = &ffma_slice->data.slots[index];
        double_linked_list_remove_item(
                ffma->slots,
                &ffma_slot->double_linked_list_item);
    }
}

void ffma_slice_make_available(
        ffma_t* ffma,
        ffma_slice_t* ffma_slice) {
    ffma_slice_remove_slots_from_per_thread_metadata_slots(
            ffma,
            ffma_slice);

    ffma->metrics.slices_inuse_count--;

    double_linked_list_remove_item(
            ffma->slices,
            &ffma_slice->double_linked_list_item);
}

void ffma_grow(
        ffma_t* ffma,
        void* memptr) {
    // Initialize the new slice and set it to non-available because it's going to be immediately used
    ffma_slice_t* ffma_slice = ffma_slice_init(
            ffma,
            memptr);
    ffma_slice->data.available = false;

    // Add just one slot to the list of slots available for the current thread
    ffma_slice_add_some_slots_to_per_thread_metadata_slots(
            ffma,
            ffma_slice);
    ffma->metrics.slices_inuse_count++;

    double_linked_list_push_item(
            ffma->slices,
            &ffma_slice->double_linked_list_item);

#if defined(HAS_VALGRIND)
    VALGRIND_CREATE_MEMPOOL(ffma_slice->data.page_addr, 0, false);
#endif
}

void ffma_mem_free_slot_in_current_thread(
        ffma_t* ffma,
        ffma_slice_t* ffma_slice,
        ffma_slot_t* ffma_slot) {
    // Update the availability and the metrics
    ffma_slice->data.metrics.objects_inuse_count--;
    ffma->metrics.objects_inuse_count--;
    ffma_slot->data.available = true;

    // Move the slot back to the head because it's available
    double_linked_list_move_item_to_head(
            ffma->slots,
            &ffma_slot->double_linked_list_item);

    // If the slice is empty check if it makes sense to free it, always keep 1 slice hot ready to be used
    if (unlikely(ffma_slice->data.metrics.objects_inuse_count == 0)) {
        // Calculate the amount of slices in use
        uint32_t total_slices_in_use =
                (ffma->metrics.objects_inuse_count + ffma_slice->data.metrics.objects_total_count - 1) /
                ffma_slice->data.metrics.objects_total_count;
        if (unlikely(ffma->metrics.slices_inuse_count - total_slices_in_use > 1)) {
            ffma_slice_make_available(ffma, ffma_slice);

#if defined(HAS_VALGRIND)
            VALGRIND_DESTROY_MEMPOOL(ffma_slice->data.page_addr);
#endif
            ffma_region_cache_push(internal_ffma_region_cache, ffma_slice->data.page_addr);
        }
    }

    MEMORY_FENCE_STORE();
}

void ffma_mem_free_slot_different_thread(
        ffma_t* ffma,
        ffma_slot_t* ffma_slot) {
    if (unlikely(!queue_mpmc_push(&ffma->free_ffma_slots_queue_from_other_threads, ffma_slot))) {
        FATAL(TAG, "Can't pass the slot to free to the owning thread, unable to continue");
    }

    // To determine which thread can clean up the data the code simply checks if objects_inuse_count - length of the
    // free_ffma_slots_queue queue is equals to 0, if it is this thread can perform the final clean up.
    // The objects_inuse_count is not atomic but memory fences are in use
    MEMORY_FENCE_LOAD();
    if (unlikely(ffma->ffma_freed)) {
        // If the last object pushed was the last that needed to be freed, ffma_free can be invoked.
        bool can_free_ffma =
                (ffma->metrics.objects_inuse_count -
                queue_mpmc_get_length(&ffma->free_ffma_slots_queue_from_other_threads)) == 0;
        if (unlikely(can_free_ffma)) {
            ffma_free(ffma);
        }
    }
}

#if FFMA_DEBUG_ALLOCS_FREES == 1
void ffma_mem_free_wrapped(
        void* memptr,
        const char *freed_by_function,
        uint32_t freed_by_line) {
#else
void ffma_mem_free(
        void* memptr) {
#endif
    if (unlikely(memptr == NULL)) {
        return;
    }

    if (unlikely(!ffma_thread_cache_has())) {
        ffma_thread_cache_set(ffma_thread_cache_init());
    }

    // Acquire the ffma_slice, the ffma, the ffma_slot and the thread_metadata related to the memory to be
    // freed. The slice holds a pointer to the memory allocator so the slot will always be put back into the correct thread
    ffma_slice_t* ffma_slice =
            ffma_slice_from_memptr(memptr);
    ffma_t* ffma =
            ffma_slice->data.ffma;
    ffma_slot_t* ffma_slot =
            ffma_slot_from_memptr(
                    ffma,
                    ffma_slice,
                    memptr);

    // Tests to catch misalignments and double frees
    assert(ffma_slot->data.memptr == memptr);
    assert(ffma_slot->data.available == false);

#if FFMA_DEBUG_ALLOCS_FREES == 1
    ffma_slot->data.allocated_freed_by.function = freed_by_function;
    ffma_slot->data.allocated_freed_by.line = freed_by_line;
#endif

#if DEBUG == 1
    ffma_slot->data.frees++;

#if defined(HAS_VALGRIND)
    VALGRIND_MEMPOOL_FREE(ffma_slice->data.page_addr, ffma_slot->data.memptr);
#endif
#endif

    // Check if the memory is owned by a different thread, the memory can't be freed directly but has to be passed to
    // the thread owning it. In cachegrand most of the allocations are freed by the owning thread.
    bool is_different_thread = ffma != ffma_thread_cache_get_ffma_by_size(
            ffma->object_size);
    if (unlikely(is_different_thread)) {
        // This is slow path as it involves always atomic ops
        ffma_mem_free_slot_different_thread(ffma, ffma_slot);
    } else {
        ffma_mem_free_slot_in_current_thread(ffma, ffma_slice, ffma_slot);
    }
}

#if FFMA_DEBUG_ALLOCS_FREES == 1
#define ffma_mem_realloc(memptr, current_size, new_size, zero_new_memory) \
    ffma_mem_realloc_wrapped(memptr, current_size, new_size, zero_new_memory, __FUNCTION__, __LINE__)
void* ffma_mem_realloc_wrapped(
        void* memptr,
        size_t current_size,
        size_t new_size,
        bool zero_new_memory,
        const char *allocated_by_function,
        uint32_t allocated_by_line) {
#else
void* ffma_mem_realloc(
        void* memptr,
        size_t current_size,
        size_t new_size,
        bool zero_new_memory) {
#endif
    // TODO: the implementation is terrible, it's not even checking if the new size fits within the provided slot
    //       because in case a new allocation is not really needed
    void* new_memptr;

#if FFMA_DEBUG_ALLOCS_FREES == 1
    new_memptr = ffma_mem_alloc_wrapped(new_size, allocated_by_function, allocated_by_line);
#else
    new_memptr = ffma_mem_alloc(new_size);
#endif

    // If the new allocation doesn't fail check if it has to be zeroed
    if (!new_memptr) {
        return new_memptr;
    }

    // Always free the pointer passed, even if the realloc fails
    if (memptr != NULL) {
        memcpy(new_memptr, memptr, current_size);
        ffma_mem_free(memptr);
    }

    if (zero_new_memory) {
        memset(new_memptr + current_size, 0, new_size - current_size);
    }

    return new_memptr;
}
