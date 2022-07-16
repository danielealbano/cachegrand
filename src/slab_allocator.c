/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>

#if __has_include(<valgrind/valgrind.h>)
#include <valgrind/valgrind.h>
#define HAS_VALGRIND
#endif

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "xalloc.h"
#include "fatal.h"
#include "utils_cpu.h"
#include "thread.h"
#include "hugepages.h"
#include "hugepage_cache.h"

#include "slab_allocator.h"

#define TAG "slab_allocator"

/**
 * The slab allocator HEAVILY relies on the hugepages, the hugepage address is 2MB aligned therefore it's possible to
 * calculate the initial address of the page and place the index of slab slice at the beginning and use the rest of the
 * page to store the data.
 */

size_t slab_os_page_size;
bool slab_allocator_enabled = false;
static pthread_key_t slab_allocator_thread_cache_key;

FUNCTION_CTOR(slab_allocator_fetch_slab_os_page_size, {
    slab_os_page_size = xalloc_get_page_size();

    pthread_key_create(&slab_allocator_thread_cache_key, slab_allocator_thread_cache_free);
})

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
        slab_allocator_t** thread_cache) {
    if (pthread_setspecific(slab_allocator_thread_cache_key, thread_cache) != 0) {
        FATAL(TAG, "Unable to set the slab allocator thread cache");
    }
}

bool slab_allocator_thread_cache_has() {
    return slab_allocator_thread_cache_get() != NULL;
}

void slab_allocator_enable(
        bool enable) {
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
    slab_allocator->thread_metadata.slots = double_linked_list_init();
    slab_allocator->thread_metadata.slices = double_linked_list_init();
    slab_allocator->thread_metadata.metrics.slices_inuse_count = 0;
    slab_allocator->thread_metadata.metrics.objects_inuse_count = 0;

    return slab_allocator;
}

void slab_allocator_free(
        slab_allocator_t* slab_allocator) {
    double_linked_list_item_t* item = slab_allocator->thread_metadata.slices->head;

    // Can't iterate using the normal double_linked_list_iter_next as the double_linked_list_item is embedded in the
    // hugepage and the hugepage is going to get freed
    while(item != NULL) {
        slab_slice_t* slab_slice = item->data;
        item = item->next;

        // If objects are in use, don't return the hugepage as it would corrupt the data in use
        if (slab_slice->data.metrics.objects_inuse_count > 0) {
            continue;
        }

        hugepage_cache_push(slab_slice->data.page_addr);
    }

    double_linked_list_free(slab_allocator->thread_metadata.slices);
    double_linked_list_free(slab_allocator->thread_metadata.slots);
    xalloc_free(slab_allocator);
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
    slab_allocator_thread_metadata_t* thread_metadata = &slab_allocator->thread_metadata;

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
                thread_metadata->slots,
                &slab_slot->double_linked_list_item);
    }
}

void slab_allocator_slice_remove_slots_from_per_thread_metadata_slots(
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice) {
    slab_allocator_thread_metadata_t* thread_metadata = &slab_allocator->thread_metadata;

    slab_slot_t* slab_slot;
    for(uint32_t index = 0; index < slab_slice->data.metrics.objects_total_count; index++) {
        slab_slot = &slab_slice->data.slots[index];
        double_linked_list_remove_item(
                thread_metadata->slots,
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
    slab_allocator_thread_metadata_t* thread_metadata = &slab_allocator->thread_metadata;

    slab_allocator_slice_remove_slots_from_per_thread_metadata_slots(
            slab_allocator,
            slab_slice);

    thread_metadata->metrics.slices_inuse_count--;

    double_linked_list_remove_item(
            thread_metadata->slices,
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
    slab_allocator_thread_metadata_t* thread_metadata = &slab_allocator->thread_metadata;

    // Initialize the new slice and set it to non available because it's going to be immediately used
    slab_slice_t* slab_slice = slab_allocator_slice_init(
            slab_allocator,
            memptr);
    slab_slice->data.available = false;

    // Add all the slots to the double linked list
    slab_allocator_slice_add_slots_to_per_thread_metadata_slots(
            slab_allocator,
            slab_slice);
    thread_metadata->metrics.slices_inuse_count++;

    double_linked_list_push_item(
            thread_metadata->slices,
            &slab_slice->double_linked_list_item);
}

void* slab_allocator_mem_alloc_hugepages(
        size_t size) {
    slab_slot_t* slab_slot = NULL;
    slab_slice_t* slab_slice = NULL;

    if (!slab_allocator_thread_cache_has()) {
        slab_allocator_thread_cache_set(slab_allocator_thread_cache_init());
    }

    uint8_t predefined_slab_allocator_index = slab_index_by_object_size(size);
    slab_allocator_t* slab_allocator = slab_allocator_thread_cache_get()[predefined_slab_allocator_index];

    slab_allocator_thread_metadata_t* thread_metadata = &slab_allocator->thread_metadata;

    double_linked_list_t* slots_list = thread_metadata->slots;
    double_linked_list_item_t* slots_head_item = slots_list->head;
    slab_slot = (slab_slot_t*)slots_head_item;

    if (slots_head_item == NULL || slab_slot->data.available == false) {
        void* hugepage_addr = hugepage_cache_pop();

#if defined(HAS_VALGRIND)
        VALGRIND_CREATE_MEMPOOL(hugepage_addr, 0, false);
#endif

        slab_allocator_grow(
                slab_allocator,
                hugepage_addr);

        slots_head_item = slots_list->head;
        slab_slot = (slab_slot_t*)slots_head_item;
        assert(slab_slot->data.memptr != NULL);
    } else {
        assert(slab_slot->data.memptr != NULL);
    }

    assert(slab_slot->data.allocs == slab_slot->data.frees);

    slab_slot->data.available = false;
#if DEBUG == 1
    slab_slot->data.allocs++;
#endif
    double_linked_list_move_item_to_tail(slots_list, slots_head_item);

    slab_slice = slab_allocator_slice_from_memptr(slab_slot->data.memptr);
    slab_slice->data.metrics.objects_inuse_count++;

    thread_metadata->metrics.objects_inuse_count++;

#if defined(HAS_VALGRIND)
    VALGRIND_MEMPOOL_ALLOC(slab_slice->data.page_addr, slab_slot->data.memptr, size);
#endif

    return slab_slot->data.memptr;
}

void* slab_allocator_mem_alloc_zero(
        size_t size) {
    void* memptr = slab_allocator_mem_alloc(size);
    memset(memptr, 0, size);

    return memptr;
}

void slab_allocator_mem_free_hugepages_local(
        void* memptr,
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice,
        slab_slot_t* slab_slot) {
    bool can_free_slab_slice = false;
    slab_allocator_thread_metadata_t* thread_metadata = &slab_allocator->thread_metadata;

    // Update the availability and the metrics
    slab_slice->data.metrics.objects_inuse_count--;
    thread_metadata->metrics.objects_inuse_count--;
    slab_slot->data.available = true;

#if DEBUG == 1
    slab_slot->data.frees++;
#endif

    // Move the slot back to the head because it's available
    double_linked_list_move_item_to_head(
            thread_metadata->slots,
            &slab_slot->double_linked_list_item);

    // If the slice is empty and for the currently core there is already another empty slice, make the current
    // slice available for other cores in the same numa node
    if (slab_slice->data.metrics.objects_inuse_count == 0) {
        slab_allocator_slice_make_available(slab_allocator, slab_slice);
        can_free_slab_slice = true;
    }

#if defined(HAS_VALGRIND)
    VALGRIND_MEMPOOL_FREE(slab_slice->data.page_addr, memptr);
#endif

    if (can_free_slab_slice) {
#if defined(HAS_VALGRIND)
        VALGRIND_DESTROY_MEMPOOL(slab_slice->data.page_addr);
#endif

        hugepage_cache_push(slab_slice->data.page_addr);
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

    // Check if the memory is owned by a different thread, if it's the case the memory can't be freed by this thread
    // but has to be passed to the thread owning it. This is more of a corner case as the most of the allocations are
    // freed by the same thread but it needs to be handled.
    if (unlikely(&slab_allocator->thread_metadata != &slab_allocator_thread_cache_get_slab_allocator_by_size(
            slab_allocator->object_size)->thread_metadata)) {
        // TODO
        assert(false);
    } else {
        slab_allocator_mem_free_hugepages_local(memptr, slab_allocator, slab_slice, slab_slot);
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

    if (slab_allocator_enabled) {
        memptr = slab_allocator_mem_alloc_hugepages(size);
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
        slab_allocator_mem_free_hugepages(memptr);
    } else {
        slab_allocator_mem_free_xalloc(memptr);
    }
}
