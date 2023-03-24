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
#include <pthread.h>

#if defined(DEBUG) &&  __has_include(<valgrind/valgrind.h>)
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

#include "ffma.h"

#if FFMA_DEBUG_ALLOCS_FREES == 1
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif

#define TAG "ffma"

/**
 * The memory allocator requires hugepages, the hugepage address is 2MB aligned therefore it's possible to calculate the
 * initial address of the page from a pointer within and from there calculate the address to the metadata stored at the
 * beginning
 */

size_t ffma_os_page_size;
bool_volatile_t ffma_enabled = false;
static pthread_key_t ffma_thread_cache_key;

#if FFMA_DEBUG_ALLOCS_FREES == 1
// When in debug mode, allow the allocated memory allocators are tracked in a queue for later checks at shutdown
queue_mpmc_t *debug_ffma_list;
#endif

FUNCTION_CTOR(ffma_general_init, {
    ffma_os_page_size = xalloc_get_page_size();

    pthread_key_create(&ffma_thread_cache_key, ffma_thread_cache_free);

#if FFMA_DEBUG_ALLOCS_FREES == 1
    debug_ffma_list = queue_mpmc_init();
#endif
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

    pid_t previous_thread_id = 0;
    while((ffma = (ffma_t*)queue_mpmc_pop(debug_ffma_list)) != NULL) {
        char thread_id_str[20] = { 0 };
        snprintf(thread_id_str, sizeof(thread_id_str) - 1, "%d", ffma->thread_id);

        uint32_t leaked_object_count =
                ffma->metrics.objects_inuse_count -
                        queue_mpmc_get_length(ffma->free_ffma_slots_queue_from_other_threads);

        if (leaked_object_count == 0) {
            continue;
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

ffma_t **ffma_thread_cache_init() {
    ffma_t **thread_ffmas = (ffma_t**)xalloc_alloc_zero(
            FFMA_PREDEFINED_OBJECT_SIZES_COUNT * (sizeof(ffma_t*) + 1));

    for(int i = 0; i < FFMA_PREDEFINED_OBJECT_SIZES_COUNT; i++) {
        uint32_t object_size = ffma_predefined_object_sizes[i];
        thread_ffmas[ffma_index_by_object_size(object_size)] = ffma_init(object_size);
    }

    return thread_ffmas;
}

void ffma_thread_cache_free(void *data) {
    ffma_t **thread_ffmas = data;

    for(int i = 0; i < FFMA_PREDEFINED_OBJECT_SIZES_COUNT; i++) {
        uint32_t object_size = ffma_predefined_object_sizes[i];
        uint32_t thread_ffmas_index = ffma_index_by_object_size(object_size);
        ffma_free(thread_ffmas[thread_ffmas_index]);
        thread_ffmas[thread_ffmas_index] = NULL;
    }

    xalloc_free(data);
}

ffma_t** ffma_thread_cache_get() {
    ffma_t **thread_ffmas = pthread_getspecific(ffma_thread_cache_key);
    return thread_ffmas;
}

void ffma_thread_cache_set(
        ffma_t** ffmas) {
    if (pthread_setspecific(ffma_thread_cache_key, ffmas) != 0) {
        FATAL(TAG, "Unable to set the fast fixed memory allocator thread cache");
    }
}

bool ffma_thread_cache_has() {
    return ffma_thread_cache_get() != NULL;
}

void ffma_enable(
        bool enable) {
    if (enable && unlikely(!ffma_thread_cache_has())) {
        ffma_thread_cache_set(ffma_thread_cache_init());
    }

    ffma_enabled = enable;
    MEMORY_FENCE_STORE();
}

bool ffma_is_enabled() {
    MEMORY_FENCE_LOAD();
    return ffma_enabled;
}

uint8_t ffma_index_by_object_size(
        size_t object_size) {
    assert(object_size <= FFMA_OBJECT_SIZE_MAX);

    if (object_size < FFMA_OBJECT_SIZE_MIN) {
        object_size = FFMA_OBJECT_SIZE_MIN;
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

    return 32 - __builtin_clz(rounded_up_object_size) - (32 - __builtin_clz(FFMA_OBJECT_SIZE_MIN));
}

ffma_t* ffma_thread_cache_get_ffma_by_size(
        size_t object_size) {
    return ffma_thread_cache_get()[ffma_index_by_object_size(object_size)];
}

ffma_t* ffma_init(
        size_t object_size) {
    assert(object_size <= FFMA_OBJECT_SIZE_MAX);

    ffma_t* ffma = (ffma_t*)xalloc_alloc_zero(sizeof(ffma_t));

    ffma->object_size = object_size;
    ffma->slots = double_linked_list_init();
    ffma->slices = double_linked_list_init();
    ffma->metrics.slices_inuse_count = 0;
    ffma->metrics.objects_inuse_count = 0;
    ffma->free_ffma_slots_queue_from_other_threads = queue_mpmc_init();

#if FFMA_DEBUG_ALLOCS_FREES == 1
    ffma->thread_id = syscall(SYS_gettid);
    pthread_getname_np(pthread_self(), ffma->thread_name, sizeof(ffma->thread_name) - 1);

    queue_mpmc_push(debug_ffma_list, ffma);
#endif

    return ffma;
}

bool ffma_free(
        ffma_t* ffma) {
    double_linked_list_item_t* item;
    ffma_slot_t *ffma_slot;

    ffma->ffma_freed = true;
    MEMORY_FENCE_STORE();

    // If there are objects in use they are most likely owned in use in some other threads and therefore the memory
    // can't be freed. The ownership of the operation fall upon the thread that will return the last object.
    // Not optimal, as it would be better to use a dying thread to free up memory instead of an in-use thread.
    // For currently cachegrand doesn't matter really because the threads are long-lived and only terminate when it
    // shuts-down.
    uint32_t objects_inuse_count =
            ffma->metrics.objects_inuse_count -
            queue_mpmc_get_length(ffma->free_ffma_slots_queue_from_other_threads);
    if (objects_inuse_count > 0) {
        return false;
    }

    // Clean up the free list
    while((ffma_slot = queue_mpmc_pop(ffma->free_ffma_slots_queue_from_other_threads)) != NULL) {
        ffma_slice_t* ffma_slice = ffma_slice_from_memptr(ffma_slot->data.memptr);
        ffma_mem_free_hugepages_current_thread(ffma, ffma_slice, ffma_slot);
    }

    // Can't iterate using the normal double_linked_list_iter_next as the double_linked_list_item is embedded in the
    // hugepage and the hugepage is going to get freed
    item = ffma->slices->head;
    while(item != NULL) {
        ffma_slice_t* ffma_slice = item->data;
        item = item->next;
        hugepage_cache_push(ffma_slice->data.page_addr);
    }

    double_linked_list_free(ffma->slices);
    double_linked_list_free(ffma->slots);

#if FFMA_DEBUG_ALLOCS_FREES == 1
    // Do nothing really, it's to ensure that the memory will get always freed if the condition checked is not 1
#else
    queue_mpmc_free(ffma->free_ffma_slots_queue_from_other_threads);
    xalloc_free(ffma);
#endif

    return true;
}

size_t ffma_slice_calculate_usable_hugepage_size() {
    size_t hugepage_size = HUGEPAGE_SIZE_2MB;
    size_t ffma_slice_size = sizeof(ffma_slice_t);
    size_t usable_hugepage_size = hugepage_size - ffma_os_page_size - ffma_slice_size;

    return usable_hugepage_size;
}

uint32_t ffma_slice_calculate_data_offset(
        size_t usable_hugepage_size,
        size_t object_size) {
    uint32_t slots_count = (int)(usable_hugepage_size / (object_size + sizeof(ffma_slot_t)));
    size_t data_offset = sizeof(ffma_slice_t) + (slots_count * sizeof(ffma_slot_t));
    data_offset += ffma_os_page_size - (data_offset % ffma_os_page_size);

    return data_offset;
}

uint32_t ffma_slice_calculate_slots_count(
        size_t usable_hugepage_size,
        size_t data_offset,
        size_t object_size) {
    size_t data_size = usable_hugepage_size - data_offset;
    uint32_t slots_count = data_size / object_size;

    return slots_count;
}

ffma_slice_t* ffma_slice_init(
        ffma_t* ffma,
        void* memptr) {
    ffma_slice_t* ffma_slice = (ffma_slice_t*)memptr;

    size_t usable_hugepage_size = ffma_slice_calculate_usable_hugepage_size();
    uint32_t data_offset = ffma_slice_calculate_data_offset(
            usable_hugepage_size,
            ffma->object_size);
    uint32_t slots_count = ffma_slice_calculate_slots_count(
            usable_hugepage_size,
            data_offset,
            ffma->object_size);

    ffma_slice->data.ffma = ffma;
    ffma_slice->data.page_addr = memptr;
    ffma_slice->data.data_addr = (uintptr_t)memptr + data_offset;
    ffma_slice->data.metrics.objects_total_count = slots_count;
    ffma_slice->data.metrics.objects_inuse_count = 0;
    ffma_slice->data.available = true;

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

ffma_slice_t* ffma_slice_from_memptr(
        void* memptr) {
    ffma_slice_t* ffma_slice = memptr - ((uintptr_t)memptr % HUGEPAGE_SIZE_2MB);
    return ffma_slice;
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

ffma_slot_t* ffma_slot_from_memptr(
        ffma_t* ffma,
        ffma_slice_t* ffma_slice,
        void* memptr) {
    uint16_t object_index = ((uintptr_t)memptr - (uintptr_t)ffma_slice->data.data_addr) / ffma->object_size;
    ffma_slot_t* slot = &ffma_slice->data.slots[object_index];

    return slot;
}

void ffma_grow(
        ffma_t* ffma,
        void* memptr) {
    // Initialize the new slice and set it to non-available because it's going to be immediately used
    ffma_slice_t* ffma_slice = ffma_slice_init(
            ffma,
            memptr);
    ffma_slice->data.available = false;

    // Add all the slots to the double linked list
    ffma_slice_add_slots_to_per_thread_metadata_slots(
            ffma,
            ffma_slice);
    ffma->metrics.slices_inuse_count++;

    double_linked_list_push_item(
            ffma->slices,
            &ffma_slice->double_linked_list_item);
}

void* ffma_mem_alloc_hugepages(
        ffma_t* ffma,
        size_t size) {
    assert(size <= FFMA_OBJECT_SIZE_MAX);

    double_linked_list_t* slots_list;
    double_linked_list_item_t* slots_head_item;
    ffma_slot_t* ffma_slot = NULL;
    ffma_slice_t* ffma_slice = NULL;

    // Always tries first to get a slow from the local cache, it's faster
    slots_list = ffma->slots;
    slots_head_item = slots_list->head;
    ffma_slot = (ffma_slot_t*)slots_head_item;

    // If it can't get the slot from the local cache tries to fetch if from the free list which is a bit slower as it
    // involves atomic operations, on the other end it requires less operation to be prepared as e.g. it is already on
    // the correct side of the slots double linked list
    if (
            (ffma_slot == NULL || ffma_slot->data.available == false) &&
            (ffma_slot = queue_mpmc_pop(ffma->free_ffma_slots_queue_from_other_threads)) != NULL) {
        assert(ffma_slot->data.memptr != NULL);
        ffma_slot->data.available = false;

#if DEBUG == 1
        ffma_slot->data.allocs++;

#if defined(HAS_VALGRIND)
        ffma_slice = ffma_slice_from_memptr(ffma_slot->data.memptr);
        VALGRIND_MEMPOOL_ALLOC(ffma_slice->data.page_addr, ffma_slot->data.memptr, size);
#endif
#endif

        // To keep the code and avoid convoluted ifs, the code returns here
        return ffma_slot->data.memptr;
    }

    if (ffma_slot == NULL || ffma_slot->data.available == false) {
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

        ffma_grow(
                ffma,
                hugepage_addr);

        slots_head_item = slots_list->head;
        ffma_slot = (ffma_slot_t*)slots_head_item;
    }

    assert(ffma_slot->data.memptr != NULL);
    assert(ffma_slot->data.allocs == ffma_slot->data.frees);

    double_linked_list_move_item_to_tail(slots_list, slots_head_item);

    ffma_slice = ffma_slice_from_memptr(ffma_slot->data.memptr);
    ffma_slice->data.metrics.objects_inuse_count++;
    ffma->metrics.objects_inuse_count++;

    ffma_slot->data.available = false;
#if DEBUG == 1
    ffma_slot->data.allocs++;

#if defined(HAS_VALGRIND)
    VALGRIND_MEMPOOL_ALLOC(ffma_slice->data.page_addr, ffma_slot->data.memptr, size);
#endif
#endif

    MEMORY_FENCE_STORE();

    return ffma_slot->data.memptr;
}

void* ffma_mem_alloc_zero(
        size_t size) {
    void* memptr = ffma_mem_alloc(size);
    if (memptr) {
        memset(memptr, 0, size);
    }

    return memptr;
}

void ffma_mem_free_hugepages_current_thread(
        ffma_t* ffma,
        ffma_slice_t* ffma_slice,
        ffma_slot_t* ffma_slot) {
    bool can_free_ffma_slice = false;
    // Update the availability and the metrics
    ffma_slice->data.metrics.objects_inuse_count--;
    ffma->metrics.objects_inuse_count--;
    ffma_slot->data.available = true;

    // Move the slot back to the head because it's available
    double_linked_list_move_item_to_head(
            ffma->slots,
            &ffma_slot->double_linked_list_item);

    // If the slice is empty and for the currently core there is already another empty slice, make the current
    // slice available for other cores in the same numa node
    if (ffma_slice->data.metrics.objects_inuse_count == 0) {
        ffma_slice_make_available(ffma, ffma_slice);
        can_free_ffma_slice = true;
    }

    if (can_free_ffma_slice) {
#if DEBUG == 1
#if defined(HAS_VALGRIND)
        VALGRIND_DESTROY_MEMPOOL(ffma_slice->data.page_addr);
#endif
#endif
        hugepage_cache_push(ffma_slice->data.page_addr);
    }

    MEMORY_FENCE_STORE();
}

void ffma_mem_free_hugepages_different_thread(
        ffma_t* ffma,
        ffma_slot_t* ffma_slot) {
    if (unlikely(!queue_mpmc_push(ffma->free_ffma_slots_queue_from_other_threads, ffma_slot))) {
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
                queue_mpmc_get_length(ffma->free_ffma_slots_queue_from_other_threads)) == 0;
        if (unlikely(can_free_ffma)) {
            ffma_free(ffma);
        }
    }
}

void ffma_mem_free_hugepages(
        void* memptr) {
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

    // Test to catch double free
    assert(ffma_slot->data.available == false);

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
        // This is slow path as it involves always atomic ops and potentially also a spinlock
        ffma_mem_free_hugepages_different_thread(ffma, ffma_slot);
    } else {
        ffma_mem_free_hugepages_current_thread(ffma, ffma_slice, ffma_slot);
    }
}

void* ffma_mem_alloc_xalloc(
        size_t size) {
    return xalloc_alloc(size);
}

void ffma_mem_free_xalloc(
        void* memptr) {
    xalloc_free(memptr);
}

void* ffma_mem_alloc(
        size_t size) {
    void* memptr;

    assert(size > 0);

    if (likely(ffma_enabled)) {
        if (unlikely(!ffma_thread_cache_has())) {
            ffma_thread_cache_set(ffma_thread_cache_init());
        }

        ffma_t *ffma = ffma_thread_cache_get()[ffma_index_by_object_size(size)];
        memptr = ffma_mem_alloc_hugepages(ffma, size);
    } else {
        memptr = ffma_mem_alloc_xalloc(size);
    }

    return memptr;
}

void* ffma_mem_realloc(
        void* memptr,
        size_t current_size,
        size_t new_size,
        bool zero_new_memory) {
    // TODO: the implementation is terrible, it's not even checking if the new size fits within the provided slot
    //       because in case a new allocation is not really needed
    void* new_memptr;

    new_memptr = ffma_mem_alloc(new_size);

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

void ffma_mem_free(
        void* memptr) {
    if (likely(ffma_enabled)) {
        if (unlikely(!ffma_thread_cache_has())) {
            ffma_thread_cache_set(ffma_thread_cache_init());
        }

        ffma_mem_free_hugepages(memptr);
    } else {
        ffma_mem_free_xalloc(memptr);
    }
}
