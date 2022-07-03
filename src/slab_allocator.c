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
#include <stdatomic.h>

#if __has_include(<valgrind/valgrind.h>)
#include <valgrind/valgrind.h>
#define HAS_VALGRIND
#endif

#include "misc.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "xalloc.h"
#include "fatal.h"
#include "utils_cpu.h"
#include "utils_numa.h"
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
slab_allocator_t* predefined_slab_allocators[SLAB_PREDEFINED_OBJECT_SIZES_COUNT] = { 0 };
bool slab_allocator_enabled = false;

FUNCTION_CTOR(slab_allocator_fetch_slab_os_page_size, {
    slab_os_page_size = xalloc_get_page_size();
})

void slab_allocator_predefined_allocators_init() {
    if (!slab_allocator_enabled) {
        FATAL(TAG, "Slab allocator disabled, unable to initialize predefined slab allocators per size");
    }

    for(int i = 0; i < SLAB_PREDEFINED_OBJECT_SIZES_COUNT; i++) {
        uint32_t object_size = slab_predefined_object_sizes[i];
        uint32_t predefined_slab_allocators_index = slab_index_by_object_size(object_size);

        predefined_slab_allocators[predefined_slab_allocators_index] =
                slab_allocator_init(SLAB_OBJECT_SIZE_MIN << i);
    }
}

void slab_allocator_predefined_allocators_free() {
    if (!slab_allocator_enabled) {
        FATAL(TAG, "Slab allocator disabled, unable to free predefined slab allocators per size");
    }

    for(int i = 0; i < SLAB_PREDEFINED_OBJECT_SIZES_COUNT; i++) {
        if (predefined_slab_allocators[i] == NULL) {
            continue;
        }

        uint32_t object_size = slab_predefined_object_sizes[i];
        uint32_t predefined_slab_allocators_index = slab_index_by_object_size(object_size);
        slab_allocator_free(predefined_slab_allocators[predefined_slab_allocators_index]);
        predefined_slab_allocators[predefined_slab_allocators_index] = NULL;
    }
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

slab_allocator_t* slab_allocator_predefined_get_by_size(
        size_t object_size) {
    return predefined_slab_allocators[slab_index_by_object_size(object_size)];
}

slab_allocator_t* slab_allocator_init(
        size_t object_size) {
    assert(object_size <= SLAB_OBJECT_SIZE_MAX);

    int numa_node_count = utils_numa_node_configured_count();
    int core_count = utils_cpu_count();

    slab_allocator_t* slab_allocator = (slab_allocator_t*)xalloc_alloc_zero(sizeof(slab_allocator_t));

    spinlock_init(&slab_allocator->spinlock);

    slab_allocator->object_size = object_size;
    slab_allocator->numa_node_count = numa_node_count;
    slab_allocator->core_count = core_count;
    slab_allocator->numa_node_metadata = xalloc_alloc_zero(sizeof(slab_allocator_numa_node_metadata_t) * numa_node_count);
    slab_allocator->core_metadata = xalloc_alloc_zero(sizeof(slab_allocator_core_metadata_t) * core_count);
    slab_allocator->metrics.total_slices_count = 0;
    slab_allocator->metrics.free_slices_count = 0;

    for(int i = 0; i < slab_allocator->core_count; i++) {
        slab_allocator_core_metadata_t* core_metadata = &slab_allocator->core_metadata[i];

        core_metadata->slots = double_linked_list_init();
        spinlock_init(&core_metadata->spinlock);

        core_metadata->metrics.slices_free_count = 0;
        core_metadata->metrics.slices_total_count = 0;
        core_metadata->metrics.slices_inuse_count = 0;
        core_metadata->metrics.objects_inuse_count = 0;
    }

    for(int i = 0; i < slab_allocator->numa_node_count; i++) {
        slab_allocator_numa_node_metadata_t* numa_node_metadata = &slab_allocator->numa_node_metadata[i];

        numa_node_metadata->slices = double_linked_list_init();

        numa_node_metadata->metrics.free_slices_count = 0;
        numa_node_metadata->metrics.total_slices_count = 0;
    }

    return slab_allocator;
}

void slab_allocator_free(
        slab_allocator_t* slab_allocator) {
    // No need to deallocate the double linked list items of the slots as they are embedded within the hugepage and the
    // hugepage is going to get freed below.

    for(int i = 0; i < slab_allocator->numa_node_count; i++) {
        slab_allocator_numa_node_metadata_t* numa_node_metadata = &slab_allocator->numa_node_metadata[i];
        double_linked_list_item_t* item = numa_node_metadata->slices->head;

        // Can't iterate using the normal double_linked_list_iter_next as the double_linked_list_item is embedded in the
        // hugepage and the hugepage is going to get freed
        while(item != NULL) {
            slab_slice_t* slab_slice = item->data;
            item = item->next;
            hugepage_cache_push(slab_slice->data.page_addr);
        }

        double_linked_list_free(numa_node_metadata->slices);
    }

    for(int i = 0; i < slab_allocator->core_count; i++) {
        slab_allocator_core_metadata_t* core_metadata = &slab_allocator->core_metadata[i];
        double_linked_list_free(core_metadata->slots);
    }

    xalloc_free(slab_allocator->core_metadata);
    xalloc_free(slab_allocator->numa_node_metadata);
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
        void* memptr,
        uint8_t numa_node_index,
        uint16_t core_index) {
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
    slab_slice->data.numa_node_index = numa_node_index;
    slab_slice->data.core_index = core_index;
    slab_slice->data.available = true;

    return slab_slice;
}

void slab_allocator_slice_add_slots_to_per_core_metadata_slots(
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice,
        uint16_t core_index) {
    slab_allocator_core_metadata_t* core_metadata = &slab_allocator->core_metadata[core_index];

    // The caller must ensure the lock
    assert(core_metadata->spinlock.lock != 0);

    slab_slot_t* slab_slot;
    for(uint32_t index = 0; index < slab_slice->data.metrics.objects_total_count; index++) {
        slab_slot = &slab_slice->data.slots[index];
        slab_slot->data.available = true;
        slab_slot->data.memptr = (void*)(slab_slice->data.data_addr + (index * slab_allocator->object_size));

        double_linked_list_unshift_item(
                core_metadata->slots,
                &slab_slot->double_linked_list_item);
    }
}

void slab_allocator_slice_remove_slots_from_per_core_metadata_slots(
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice,
        uint16_t core_index) {
    slab_allocator_core_metadata_t* core_metadata = &slab_allocator->core_metadata[core_index];

    // The caller must ensure the lock
    assert(core_metadata->spinlock.lock != 0);

    slab_slot_t* slab_slot;
    for(uint32_t index = 0; index < slab_slice->data.metrics.objects_total_count; index++) {
        slab_slot = &slab_slice->data.slots[index];
        double_linked_list_remove_item(
                core_metadata->slots,
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
        slab_slice_t* slab_slice,
        uint8_t numa_node_index,
        uint16_t core_index,
        bool* can_free_slice) {
    *can_free_slice = false;
    slab_allocator_numa_node_metadata_t* numa_node_metadata = &slab_allocator->numa_node_metadata[numa_node_index];
    slab_allocator_core_metadata_t* core_metadata = &slab_allocator->core_metadata[core_index];

    slab_allocator_slice_remove_slots_from_per_core_metadata_slots(
            slab_allocator,
            slab_slice,
            core_index);

    // The caller must ensure the lock
    assert(core_metadata->spinlock.lock != 0);

    core_metadata->metrics.slices_total_count--;
    core_metadata->metrics.slices_free_count--;

    spinlock_lock(&slab_allocator->spinlock, true);

    if (slab_allocator->metrics.free_slices_count == 1) {
        *can_free_slice = true;

        slab_allocator->metrics.total_slices_count--;
        numa_node_metadata->metrics.total_slices_count--;
        double_linked_list_remove_item(
                numa_node_metadata->slices,
                &slab_slice->double_linked_list_item);
    } else {
        slab_allocator->metrics.free_slices_count++;
        numa_node_metadata->metrics.free_slices_count++;
        slab_slice->data.available = true;
        double_linked_list_move_item_to_head(
                numa_node_metadata->slices,
                &slab_slice->double_linked_list_item);
    }

    spinlock_unlock(&slab_allocator->spinlock);
}

bool slab_allocator_slice_try_acquire(
        slab_allocator_t* slab_allocator,
        uint8_t numa_node_index,
        uint16_t core_index) {
    slab_allocator_numa_node_metadata_t* numa_node_metadata = &slab_allocator->numa_node_metadata[numa_node_index];
    slab_allocator_core_metadata_t* core_metadata = &slab_allocator->core_metadata[core_index];

    bool slab_slice_found = false;
    double_linked_list_item_t* slab_slices_per_numa_node_head_item;
    slab_slice_t* head_slab_slice;

    // Check if an existing slices is available
    spinlock_lock(&slab_allocator->spinlock, true);

    slab_slices_per_numa_node_head_item =
           numa_node_metadata->slices->head;
    head_slab_slice = (slab_slice_t *)slab_slices_per_numa_node_head_item;

    if (slab_slices_per_numa_node_head_item != NULL && head_slab_slice->data.available == true) {
        slab_slice_found = true;
        slab_allocator->metrics.free_slices_count--;
        numa_node_metadata->metrics.free_slices_count--;
        head_slab_slice->data.available = false;
        double_linked_list_move_item_to_tail(
               numa_node_metadata->slices,
                slab_slices_per_numa_node_head_item);
    }
    spinlock_unlock(&slab_allocator->spinlock);

    if (slab_slice_found) {
        core_metadata->metrics.slices_inuse_count++;
        core_metadata->metrics.slices_total_count++;
        slab_allocator_slice_add_slots_to_per_core_metadata_slots(
                slab_allocator,
                head_slab_slice,
                core_index);
    }

    return slab_slice_found;
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
        uint8_t numa_node_index,
        uint16_t core_index,
        void* memptr) {
    slab_allocator_numa_node_metadata_t* numa_node_metadata = &slab_allocator->numa_node_metadata[numa_node_index];
    slab_allocator_core_metadata_t* core_metadata = &slab_allocator->core_metadata[core_index];

    // The caller must ensure the lock on core_metadata
    assert(core_metadata->spinlock.lock != 0);

    // Initialize the new slice and set it to non available because it's going to be immediately used
    slab_slice_t* slab_slice = slab_allocator_slice_init(
            slab_allocator,
            memptr,
            numa_node_index,
            core_index);
    slab_slice->data.available = false;

    // Add all the slots to the double linked list
    slab_allocator_slice_add_slots_to_per_core_metadata_slots(
            slab_allocator,
            slab_slice,
            core_index);
    core_metadata->metrics.slices_inuse_count++;
    core_metadata->metrics.slices_total_count++;

    // Add the slice to the ones initialized per core
    spinlock_lock(&slab_allocator->spinlock, true);

    slab_allocator->metrics.total_slices_count++;
    numa_node_metadata->metrics.total_slices_count++;
    double_linked_list_push_item(
            numa_node_metadata->slices,
            &slab_slice->double_linked_list_item);

    spinlock_unlock(&slab_allocator->spinlock);
}

void* slab_allocator_mem_alloc_hugepages(
        size_t size,
        uint8_t numa_node_index,
        uint16_t core_index) {
    slab_slot_t* slab_slot = NULL;
    slab_slice_t* slab_slice = NULL;

    uint8_t predefined_slab_allocator_index = slab_index_by_object_size(size);
    slab_allocator_t* slab_allocator = predefined_slab_allocators[predefined_slab_allocator_index];

    slab_allocator_core_metadata_t* core_metadata = &slab_allocator->core_metadata[core_index];

    spinlock_lock(&core_metadata->spinlock, true);

    double_linked_list_t* slots_per_core_list = core_metadata->slots;
    double_linked_list_item_t* slots_per_core_head_item = slots_per_core_list->head;
    slab_slot = (slab_slot_t*)slots_per_core_head_item;

    if (slots_per_core_head_item == NULL || slab_slot->data.available == false) {
        if (slab_allocator_slice_try_acquire(slab_allocator, numa_node_index, core_index) == false) {
            void* hugepage_addr = hugepage_cache_pop();

#if defined(HAS_VALGRIND)
            VALGRIND_CREATE_MEMPOOL(hugepage_addr, 0, false);
#endif

            slab_allocator_grow(
                    slab_allocator,
                    numa_node_index,
                    core_index,
                    hugepage_addr);
        }

        slots_per_core_head_item = slots_per_core_list->head;
        slab_slot = (slab_slot_t*)slots_per_core_head_item;
        assert(slab_slot->data.memptr != NULL);
    } else {
        assert(slab_slot->data.memptr != NULL);
    }

    slab_slot->data.available = false;
    double_linked_list_move_item_to_tail(slots_per_core_list, slots_per_core_head_item);

    slab_slice = slab_allocator_slice_from_memptr(slab_slot->data.memptr);
    slab_slice->data.metrics.objects_inuse_count++;

    core_metadata->metrics.objects_inuse_count++;

    spinlock_unlock(&core_metadata->spinlock);

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

void slab_allocator_mem_free_hugepages(
        void* memptr) {
    bool can_free_slab_slice = false;

    // Acquire the slab_slice, the slab_allocator, the slab_slot and the core_metadata for the current core on which
    // the thread is running
    slab_slice_t* slab_slice = slab_allocator_slice_from_memptr(memptr);
    slab_allocator_t* slab_allocator = slab_slice->data.slab_allocator;
    slab_slot_t* slot = slab_allocator_slot_from_memptr(
            slab_allocator,
            slab_slice,
            memptr);
    slab_allocator_core_metadata_t* core_metadata = &slab_allocator->core_metadata[slab_slice->data.core_index];

    // Debug test to catch double free
    assert(slot->data.available == false);

    spinlock_lock(&core_metadata->spinlock, true);
    // Update the availability and the metrics
    slab_slice->data.metrics.objects_inuse_count--;
    core_metadata->metrics.objects_inuse_count--;
    slot->data.available = true;

    // Move the slot back to the head because it's available
    double_linked_list_move_item_to_head(
            core_metadata->slots,
            &slot->double_linked_list_item);

    // If the slice is empty and for the currently core there is already another empty slice, make the current
    // slice available for other cores in the same numa node
    if (slab_slice->data.metrics.objects_inuse_count == 0) {
        core_metadata->metrics.slices_free_count++;
        core_metadata->metrics.slices_inuse_count--;

        if (core_metadata->metrics.slices_free_count > 1) {
            slab_allocator_slice_make_available(
                    slab_allocator,
                    slab_slice,
                    slab_slice->data.numa_node_index,
                    slab_slice->data.core_index,
                    &can_free_slab_slice);
        }
    }
    spinlock_unlock(&core_metadata->spinlock);

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
        memptr = slab_allocator_mem_alloc_hugepages(
                size,
                thread_get_current_numa_node_index(),
                thread_get_current_core_index());
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
    //       because in case nothing is needed
    void* new_memptr;

    new_memptr = slab_allocator_mem_alloc(new_size);

    if (zero_new_memory) {
        memset(new_memptr + current_size, 0, new_size - current_size);
    }

    if (memptr != NULL) {
        memcpy(new_memptr, memptr, current_size);
        slab_allocator_mem_free(memptr);
    }

    return new_memptr;
}

void slab_allocator_mem_free(
        void* memptr) {
    if (slab_allocator_enabled) {
        slab_allocator_mem_free_hugepages(
                memptr);
    } else {
        slab_allocator_mem_free_xalloc(memptr);
    }
}
