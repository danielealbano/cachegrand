/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

// In this special debug mode the slab allocators are not freed up in slab_allocator_free but instead in the dtor
// invoked at the end of the execution to be able to access the metrics and other information.
// This might hide bugs and/or mess-up valgrind therefore it's controlled by a specific define that has to be set to 1
// inside slab_allocator.c for safety reasons
#if SLAB_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
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

#include "slab_allocator.h"

#if SLAB_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif

#define TAG "slab_allocator"

/**
 * The slab allocator requires hugepages, the hugepage address is 2MB aligned therefore it's possible to calculate the
 * initial address of the page from a pointer within and from there calculate the address to the metadata stored at the
 * beginning
 */

size_t slab_os_page_size;
bool slab_allocator_enabled = false;
static pthread_key_t slab_allocator_thread_cache_key;

#if SLAB_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
// When in debug mode, allow the allocated slab allocators are tracked in a queue for later checks at shutdown
queue_mpmc_t *debug_slab_allocator_list;
#endif

FUNCTION_CTOR(slab_allocator_general_init, {
    slab_os_page_size = xalloc_get_page_size();

    pthread_key_create(&slab_allocator_thread_cache_key, slab_allocator_thread_cache_free);

#if SLAB_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
    debug_slab_allocator_list = queue_mpmc_init();
#endif
})

#if SLAB_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
void slab_allocator_debug_allocs_frees_end() {
    slab_allocator_t *slab_allocator;
    fprintf(stdout, "> debug_slab_allocator_list length <%d>\n", queue_mpmc_get_length(debug_slab_allocator_list));
    fflush(stdout);

    fprintf(stdout, "+-%.10s-+-%.20s-+-%.14s-+-%.10s-+-%.11s-+-%.10s-+-%.12s-+-%.14s-+\n",
            "-------------------------------", "-------------------------------", "-------------------------------",
            "-------------------------------", "-------------------------------", "-------------------------------",
            "-------------------------------", "-------------------------------");

    fprintf(stdout, "| %-10s | %-20s | %-14s | %-10s | %-11s | %-10s | %-12s | %-14s |\n",
            "Thread Id", "Thread Name", "Slab Allocator",
            "Obj Size", "Leaks Count", "Obj Count",
            "Slices Count", "Free queue Len");

    fprintf(stdout, "+-%.10s-+-%.20s-+-%.14s-+-%.10s-+-%.11s-+-%.10s-+-%.12s-+-%.14s-+\n",
            "-------------------------------", "-------------------------------", "-------------------------------",
            "-------------------------------", "-------------------------------", "-------------------------------",
            "-------------------------------", "-------------------------------");

    pid_t previous_thread_id = 0;
    while((slab_allocator = (slab_allocator_t*)queue_mpmc_pop(debug_slab_allocator_list)) != NULL) {
        char thread_id_str[20] = { 0 };
        snprintf(thread_id_str, sizeof(thread_id_str) - 1, "%d", slab_allocator->thread_id);

        uint32_t leaked_object_count =
                slab_allocator->metrics.objects_inuse_count -
                        queue_mpmc_get_length(slab_allocator->free_slab_slots_queue_from_other_threads);

        if (leaked_object_count == 0) {
            continue;
        }

        fprintf(stdout, "| %-10s | %-20s | %p | %10u | %11u | %10u | %12u | %14u |\n",
                previous_thread_id == slab_allocator->thread_id ? "" : thread_id_str,
                previous_thread_id == slab_allocator->thread_id ? "" : slab_allocator->thread_name,
                slab_allocator,
                slab_allocator->object_size,
                leaked_object_count,
                slab_allocator->metrics.objects_inuse_count,
                slab_allocator->metrics.slices_inuse_count,
                queue_mpmc_get_length(slab_allocator->free_slab_slots_queue_from_other_threads));
        fflush(stdout);

        queue_mpmc_free(slab_allocator->free_slab_slots_queue_from_other_threads);
        xalloc_free(slab_allocator);

        previous_thread_id = slab_allocator->thread_id;
    }

    fprintf(stdout, "+-%.10s-+-%.20s-+-%.14s-+-%.10s-+-%.11s-+-%.10s-+-%.12s-+-%.14s-+\n",
            "-------------------------------", "-------------------------------", "-------------------------------",
            "-------------------------------", "-------------------------------", "-------------------------------",
            "-------------------------------", "-------------------------------");
}
#endif

slab_allocator_t **slab_allocator_thread_cache_init() {
    slab_allocator_t **thread_slab_allocators = (slab_allocator_t**)xalloc_alloc_zero(
            SLAB_PREDEFINED_OBJECT_SIZES_COUNT * (sizeof(slab_allocator_t*) + 1));

    for(int i = 0; i < SLAB_PREDEFINED_OBJECT_SIZES_COUNT; i++) {
        uint32_t object_size = slab_predefined_object_sizes[i];
        thread_slab_allocators[slab_index_by_object_size(object_size)] = slab_allocator_init(object_size);
    }

    return thread_slab_allocators;
}

void slab_allocator_thread_cache_free(void *data) {
    slab_allocator_t **thread_slab_allocators = data;

    for(int i = 0; i < SLAB_PREDEFINED_OBJECT_SIZES_COUNT; i++) {
        uint32_t object_size = slab_predefined_object_sizes[i];
        uint32_t thread_slab_allocators_index = slab_index_by_object_size(object_size);
        slab_allocator_free(thread_slab_allocators[thread_slab_allocators_index]);
        thread_slab_allocators[thread_slab_allocators_index] = NULL;
    }

    xalloc_free(data);
}

slab_allocator_t** slab_allocator_thread_cache_get() {
    slab_allocator_t **thread_slab_allocators = pthread_getspecific(slab_allocator_thread_cache_key);
    return thread_slab_allocators;
}

void slab_allocator_thread_cache_set(
        slab_allocator_t** slab_allocators) {
    if (pthread_setspecific(slab_allocator_thread_cache_key, slab_allocators) != 0) {
        FATAL(TAG, "Unable to set the slab allocator thread cache");
    }
}

bool slab_allocator_thread_cache_has() {
    return slab_allocator_thread_cache_get() != NULL;
}

void slab_allocator_enable(
        bool enable) {
    if (unlikely(!slab_allocator_thread_cache_has())) {
        slab_allocator_thread_cache_set(slab_allocator_thread_cache_init());
    }

    slab_allocator_enabled = enable;
}

uint8_t slab_index_by_object_size(
        size_t object_size) {
    assert(object_size <= SLAB_OBJECT_SIZE_MAX);

    if (object_size < SLAB_OBJECT_SIZE_MIN) {
        object_size = SLAB_OBJECT_SIZE_MIN;
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

    return 32 - __builtin_clz(rounded_up_object_size) - (32 - __builtin_clz(SLAB_OBJECT_SIZE_MIN));
}

slab_allocator_t* slab_allocator_thread_cache_get_slab_allocator_by_size(
        size_t object_size) {
    return slab_allocator_thread_cache_get()[slab_index_by_object_size(object_size)];
}

slab_allocator_t* slab_allocator_init(
        size_t object_size) {
    assert(object_size <= SLAB_OBJECT_SIZE_MAX);

    slab_allocator_t* slab_allocator = (slab_allocator_t*)xalloc_alloc_zero(sizeof(slab_allocator_t));

    slab_allocator->object_size = object_size;
    slab_allocator->slots = double_linked_list_init();
    slab_allocator->slices = double_linked_list_init();
    slab_allocator->metrics.slices_inuse_count = 0;
    slab_allocator->metrics.objects_inuse_count = 0;
    slab_allocator->free_slab_slots_queue_from_other_threads = queue_mpmc_init();

#if SLAB_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
    slab_allocator->thread_id = syscall(SYS_gettid);
    pthread_getname_np(pthread_self(), slab_allocator->thread_name, sizeof(slab_allocator->thread_name) - 1);

    queue_mpmc_push(debug_slab_allocator_list, slab_allocator);
#endif

    return slab_allocator;
}

bool slab_allocator_free(
        slab_allocator_t* slab_allocator) {
    double_linked_list_item_t* item;
    slab_slot_t *slab_slot;

    slab_allocator->slab_allocator_freed = true;
    MEMORY_FENCE_STORE();

    // If there are objects in use they are most likely owned in use in some other threads and therefore the memory
    // can't be freed. The ownership of the operation fall upon the thread that will return the last object.
    // Not optimal, as it would be better to use a dying thread to free up memory instead of an in-use thread.
    // For currently cachegrand doesn't matter really because the threads are long-lived and only terminate when it
    // shuts-down.
    uint32_t objects_inuse_count =
            slab_allocator->metrics.objects_inuse_count -
            queue_mpmc_get_length(slab_allocator->free_slab_slots_queue_from_other_threads);
    if (objects_inuse_count > 0) {
        return false;
    }

    // Clean up the free list
    while((slab_slot = queue_mpmc_pop(slab_allocator->free_slab_slots_queue_from_other_threads)) != NULL) {
        slab_slice_t* slab_slice = slab_allocator_slice_from_memptr(slab_slot->data.memptr);
        slab_allocator_mem_free_hugepages_current_thread(slab_allocator, slab_slice, slab_slot);
    }

    // Can't iterate using the normal double_linked_list_iter_next as the double_linked_list_item is embedded in the
    // hugepage and the hugepage is going to get freed
    item = slab_allocator->slices->head;
    while(item != NULL) {
        slab_slice_t* slab_slice = item->data;
        item = item->next;
        hugepage_cache_push(slab_slice->data.page_addr);
    }

    double_linked_list_free(slab_allocator->slices);
    double_linked_list_free(slab_allocator->slots);

#if SLAB_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
    // Do nothing really, it's to ensure that the memory will get always freed if the condition checked is not 1
#else
    queue_mpmc_free(slab_allocator->free_slab_slots_queue_from_other_threads);
    xalloc_free(slab_allocator);
#endif

    return true;
}

size_t slab_allocator_slice_calculate_usable_hugepage_size() {
    size_t hugepage_size = HUGEPAGE_SIZE_2MB;
    size_t slab_slice_size = sizeof(slab_slice_t);
    size_t usable_hugepage_size = hugepage_size - slab_os_page_size - slab_slice_size;

    return usable_hugepage_size;
}

uint32_t slab_allocator_slice_calculate_data_offset(
        size_t usable_hugepage_size,
        size_t object_size) {
    uint32_t slots_count = (int)(usable_hugepage_size / (object_size + sizeof(slab_slot_t)));
    size_t data_offset = sizeof(slab_slice_t) + (slots_count * sizeof(slab_slot_t));
    data_offset += slab_os_page_size - (data_offset % slab_os_page_size);

    return data_offset;
}

uint32_t slab_allocator_slice_calculate_slots_count(
        size_t usable_hugepage_size,
        size_t data_offset,
        size_t object_size) {
    size_t data_size = usable_hugepage_size - data_offset;
    uint32_t slots_count = data_size / object_size;

    return slots_count;
}

slab_slice_t* slab_allocator_slice_init(
        slab_allocator_t* slab_allocator,
        void* memptr) {
    slab_slice_t* slab_slice = (slab_slice_t*)memptr;

    size_t usable_hugepage_size = slab_allocator_slice_calculate_usable_hugepage_size();
    uint32_t data_offset = slab_allocator_slice_calculate_data_offset(
            usable_hugepage_size,
            slab_allocator->object_size);
    uint32_t slots_count = slab_allocator_slice_calculate_slots_count(
            usable_hugepage_size,
            data_offset,
            slab_allocator->object_size);

    slab_slice->data.slab_allocator = slab_allocator;
    slab_slice->data.page_addr = memptr;
    slab_slice->data.data_addr = (uintptr_t)memptr + data_offset;
    slab_slice->data.metrics.objects_total_count = slots_count;
    slab_slice->data.metrics.objects_inuse_count = 0;
    slab_slice->data.available = true;

    return slab_slice;
}

void slab_allocator_slice_add_slots_to_per_thread_metadata_slots(
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice) {
    slab_slot_t* slab_slot;
    for(uint32_t index = 0; index < slab_slice->data.metrics.objects_total_count; index++) {
        slab_slot = &slab_slice->data.slots[index];
        slab_slot->data.available = true;
        slab_slot->data.memptr = (void*)(slab_slice->data.data_addr + (index * slab_allocator->object_size));

#if DEBUG == 1
        slab_slot->data.allocs = 0;
        slab_slot->data.frees = 0;
#endif

        double_linked_list_unshift_item(
                slab_allocator->slots,
                &slab_slot->double_linked_list_item);
    }
}

void slab_allocator_slice_remove_slots_from_per_thread_metadata_slots(
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice) {
    slab_slot_t* slab_slot;
    for(uint32_t index = 0; index < slab_slice->data.metrics.objects_total_count; index++) {
        slab_slot = &slab_slice->data.slots[index];
        double_linked_list_remove_item(
                slab_allocator->slots,
                &slab_slot->double_linked_list_item);
    }
}

slab_slice_t* slab_allocator_slice_from_memptr(
        void* memptr) {
    slab_slice_t* slab_slice = memptr - ((uintptr_t)memptr % HUGEPAGE_SIZE_2MB);
    return slab_slice;
}

void slab_allocator_slice_make_available(
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice) {
    slab_allocator_slice_remove_slots_from_per_thread_metadata_slots(
            slab_allocator,
            slab_slice);

    slab_allocator->metrics.slices_inuse_count--;

    double_linked_list_remove_item(
            slab_allocator->slices,
            &slab_slice->double_linked_list_item);
}

slab_slot_t* slab_allocator_slot_from_memptr(
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice,
        void* memptr) {
    uint16_t object_index = ((uintptr_t)memptr - (uintptr_t)slab_slice->data.data_addr) / slab_allocator->object_size;
    slab_slot_t* slot = &slab_slice->data.slots[object_index];

    return slot;
}

void slab_allocator_grow(
        slab_allocator_t* slab_allocator,
        void* memptr) {
    // Initialize the new slice and set it to non-available because it's going to be immediately used
    slab_slice_t* slab_slice = slab_allocator_slice_init(
            slab_allocator,
            memptr);
    slab_slice->data.available = false;

    // Add all the slots to the double linked list
    slab_allocator_slice_add_slots_to_per_thread_metadata_slots(
            slab_allocator,
            slab_slice);
    slab_allocator->metrics.slices_inuse_count++;

    double_linked_list_push_item(
            slab_allocator->slices,
            &slab_slice->double_linked_list_item);
}

void* slab_allocator_mem_alloc_hugepages(
        slab_allocator_t* slab_allocator,
        size_t size) {
    double_linked_list_t* slots_list;
    double_linked_list_item_t* slots_head_item;
    slab_slot_t* slab_slot = NULL;
    slab_slice_t* slab_slice = NULL;

    // Always tries first to get a slow from the local cache, it's faster
    slots_list = slab_allocator->slots;
    slots_head_item = slots_list->head;
    slab_slot = (slab_slot_t*)slots_head_item;

    // If it can't get the slot from the local cache tries to fetch if from the free list which is a bit slower as it
    // involves atomic operations, on the other end it requires less operation to be prepared as e.g. it is already on
    // the correct side of the slots double linked list
    if (
            (slab_slot == NULL || slab_slot->data.available == false) &&
            (slab_slot = queue_mpmc_pop(slab_allocator->free_slab_slots_queue_from_other_threads)) != NULL) {
        assert(slab_slot->data.memptr != NULL);
        slab_slot->data.available = false;

#if DEBUG == 1
        slab_slot->data.allocs++;

#if defined(HAS_VALGRIND)
        slab_slice = slab_allocator_slice_from_memptr(slab_slot->data.memptr);
        VALGRIND_MEMPOOL_ALLOC(slab_slice->data.page_addr, slab_slot->data.memptr, size);
#endif
#endif

        // To keep the code and avoid convoluted ifs, the code returns here
        return slab_slot->data.memptr;
    }

    if (slab_slot == NULL || slab_slot->data.available == false) {
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

        slab_allocator_grow(
                slab_allocator,
                hugepage_addr);

        slots_head_item = slots_list->head;
        slab_slot = (slab_slot_t*)slots_head_item;
    }

    assert(slab_slot->data.memptr != NULL);
    assert(slab_slot->data.allocs == slab_slot->data.frees);

    double_linked_list_move_item_to_tail(slots_list, slots_head_item);

    slab_slice = slab_allocator_slice_from_memptr(slab_slot->data.memptr);
    slab_slice->data.metrics.objects_inuse_count++;
    slab_allocator->metrics.objects_inuse_count++;

    slab_slot->data.available = false;
#if DEBUG == 1
    slab_slot->data.allocs++;

#if defined(HAS_VALGRIND)
    VALGRIND_MEMPOOL_ALLOC(slab_slice->data.page_addr, slab_slot->data.memptr, size);
#endif
#endif

    MEMORY_FENCE_STORE();

    return slab_slot->data.memptr;
}

void* slab_allocator_mem_alloc_zero(
        size_t size) {
    void* memptr = slab_allocator_mem_alloc(size);
    memset(memptr, 0, size);

    return memptr;
}

void slab_allocator_mem_free_hugepages_current_thread(
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice,
        slab_slot_t* slab_slot) {
    bool can_free_slab_slice = false;
    // Update the availability and the metrics
    slab_slice->data.metrics.objects_inuse_count--;
    slab_allocator->metrics.objects_inuse_count--;
    slab_slot->data.available = true;

    // Move the slot back to the head because it's available
    double_linked_list_move_item_to_head(
            slab_allocator->slots,
            &slab_slot->double_linked_list_item);

    // If the slice is empty and for the currently core there is already another empty slice, make the current
    // slice available for other cores in the same numa node
    if (slab_slice->data.metrics.objects_inuse_count == 0) {
        slab_allocator_slice_make_available(slab_allocator, slab_slice);
        can_free_slab_slice = true;
    }

    if (can_free_slab_slice) {
#if DEBUG == 1
#if defined(HAS_VALGRIND)
        VALGRIND_DESTROY_MEMPOOL(slab_slice->data.page_addr);
#endif
#endif
        hugepage_cache_push(slab_slice->data.page_addr);
    }

    MEMORY_FENCE_STORE();
}

void slab_allocator_mem_free_hugepages_different_thread(
        slab_allocator_t* slab_allocator,
        slab_slot_t* slab_slot) {
    if (unlikely(!queue_mpmc_push(slab_allocator->free_slab_slots_queue_from_other_threads, slab_slot))) {
        FATAL(TAG, "Can't pass the slab slot to free to the owning thread, unable to continue");
    }

    // To determine which thread can clean up the data the code simply checks if objects_inuse_count - length of the
    // free_slab_slots_queue queue is equals to 0, if it is this thread can perform the final clean up.
    // The objects_inuse_count is not atomic but memory fences are in use
    MEMORY_FENCE_LOAD();
    if (unlikely(slab_allocator->slab_allocator_freed)) {
        // If the last object pushed was the last that needed to be freed, slab_allocator_free can be invoked.
        bool can_free_slab_allocator =
                (slab_allocator->metrics.objects_inuse_count -
                queue_mpmc_get_length(slab_allocator->free_slab_slots_queue_from_other_threads)) == 0;
        if (unlikely(can_free_slab_allocator)) {
            slab_allocator_free(slab_allocator);
        }
    }
}

void slab_allocator_mem_free_hugepages(
        void* memptr) {
    // Acquire the slab_slice, the slab_allocator, the slab_slot and the thread_metadata related to the memory to be
    // freed. The slab slice holds a pointer to the slab allocator so the slot will always be put back into the
    // correct thread
    slab_slice_t* slab_slice = slab_allocator_slice_from_memptr(memptr);
    slab_allocator_t* slab_allocator = slab_slice->data.slab_allocator;
    slab_slot_t* slab_slot = slab_allocator_slot_from_memptr(slab_allocator, slab_slice, memptr);

    // Test to catch double free
    assert(slab_slot->data.available == false);

#if DEBUG == 1
    slab_slot->data.frees++;

#if defined(HAS_VALGRIND)
    VALGRIND_MEMPOOL_FREE(slab_slice->data.page_addr, slab_slot->data.memptr);
#endif
#endif

    // Check if the memory is owned by a different thread, the memory can't be freed directly but has to be passed to
    // the thread owning it. In cachegrand most of the allocations are freed by the owning thread.
    bool is_different_thread = slab_allocator != slab_allocator_thread_cache_get_slab_allocator_by_size(
            slab_allocator->object_size);
    if (unlikely(is_different_thread)) {
        // This is slow path as it involves always atomic ops and potentially also a spinlock
        slab_allocator_mem_free_hugepages_different_thread(slab_allocator, slab_slot);
    } else {
        slab_allocator_mem_free_hugepages_current_thread(slab_allocator, slab_slice, slab_slot);
    }
}

void* slab_allocator_mem_alloc_xalloc(
        size_t size) {
    return xalloc_alloc(size);
}

void slab_allocator_mem_free_xalloc(
        void* memptr) {
    xalloc_free(memptr);
}

void* slab_allocator_mem_alloc(
        size_t size) {
    void* memptr;

    if (likely(slab_allocator_enabled)) {
        if (unlikely(!slab_allocator_thread_cache_has())) {
            slab_allocator_thread_cache_set(slab_allocator_thread_cache_init());
        }

        slab_allocator_t *slab_allocator = slab_allocator_thread_cache_get()[slab_index_by_object_size(size)];
        memptr = slab_allocator_mem_alloc_hugepages(slab_allocator, size);
    } else {
        memptr = slab_allocator_mem_alloc_xalloc(size);
    }

    return memptr;
}

void* slab_allocator_mem_realloc(
        void* memptr,
        size_t current_size,
        size_t new_size,
        bool zero_new_memory) {
    // TODO: the implementation is terrible, it's not even checking if the new size fits within the provided slot
    //       because in case a new allocation is not really needed
    void* new_memptr;

    new_memptr = slab_allocator_mem_alloc(new_size);

    // If the new allocation doesn't fail check if it has to be zeroed
    if (!new_memptr) {
        if (zero_new_memory) {
            memset(new_memptr + current_size, 0, new_size - current_size);
        }
    }

    // Always free the pointer passed, even if the realloc fails
    if (memptr != NULL) {
        memcpy(new_memptr, memptr, current_size);
        slab_allocator_mem_free(memptr);
    }

    return new_memptr;
}

void slab_allocator_mem_free(
        void* memptr) {
    if (likely(slab_allocator_enabled)) {
        if (unlikely(!slab_allocator_thread_cache_has())) {
            slab_allocator_thread_cache_set(slab_allocator_thread_cache_init());
        }

        slab_allocator_mem_free_hugepages(memptr);
    } else {
        slab_allocator_mem_free_xalloc(memptr);
    }
}
