#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <sched.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdatomic.h>

#include "misc.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "xalloc.h"
#include "log/log.h"
#include "fatal.h"
#include "hugepages.h"
#include "utils_cpu.h"
#include "utils_numa.h"
#include "data_structures/double_linked_list/double_linked_list.h"

#include "slab_allocator.h"

#define TAG "slab_allocator"

/**
 * The slab allocator HEAVILY relies on the hugepages, the hugepage address is 2MB aligned therefore it's possible to
 * calculate the initial address of the page and place the index of slab slice at the beginning and use the rest of the
 * page to store the data.
 */

#define SLAB_PAGE_2MB   (2 * 1024 * 1024)

thread_local uint32_t current_thread_core_index = UINT32_MAX;
thread_local uint32_t current_thread_numa_node_index = UINT32_MAX;

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
                slab_allocator_init(SLAB_OBJECT_SIZE_64 << i);
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

void slab_allocator_ensure_core_index_and_numa_node_index_filled() {
    if (current_thread_core_index == UINT32_MAX || current_thread_numa_node_index == UINT32_MAX) {
        getcpu(&current_thread_core_index, &current_thread_numa_node_index);
    }
}

uint8_t slab_index_by_object_size(
        size_t object_size) {
    assert(object_size <= SLAB_OBJECT_SIZE_MAX);

    if (object_size < SLAB_OBJECT_SIZE_64) {
        object_size = SLAB_OBJECT_SIZE_64;
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

    return 32 - __builtin_clz(rounded_up_object_size) - 7;
}

slab_allocator_t* slab_allocator_predefined_get_by_size(
        size_t object_size) {
    return predefined_slab_allocators[slab_index_by_object_size(object_size)];
}

uint32_t slab_allocator_get_current_thread_numa_node_index() {
    slab_allocator_ensure_core_index_and_numa_node_index_filled();
    return current_thread_numa_node_index;
}

uint32_t slab_allocator_get_current_thread_core_index() {
    slab_allocator_ensure_core_index_and_numa_node_index_filled();
    return current_thread_core_index;
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

        // TODO: Not numa-friendly, the memory has to be allocated on the correct numa node
        // TODO: to speed the first allocation a lot of memory get wasted! Needs to be changed.
        core_metadata->free_page_addr = NULL;
        core_metadata->free_page_addr_first_inited = false;

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
            xalloc_hugepages_free(slab_slice->data.page_addr, SLAB_PAGE_2MB);
        }

        double_linked_list_free(numa_node_metadata->slices);
    }

    for(int i = 0; i < slab_allocator->core_count; i++) {
        slab_allocator_core_metadata_t* core_metadata = &slab_allocator->core_metadata[i];
        double_linked_list_free(core_metadata->slots);

        // If a thread is still allocating an hugepage while the slab allocator is getting freed wait for it to avoid
        // losing it
        do {
            HASHTABLE_MEMORY_FENCE_LOAD();
        } while (core_metadata->free_page_addr == NULL && core_metadata->free_page_addr_first_inited == true);

        xalloc_hugepages_free(core_metadata->free_page_addr, SLAB_PAGE_2MB);
    }

    xalloc_free(slab_allocator->core_metadata);
    xalloc_free(slab_allocator->numa_node_metadata);
    xalloc_free(slab_allocator);
}

slab_slice_t* slab_allocator_slice_init(
        slab_allocator_t* slab_allocator,
        void* memptr) {
    void* data_addr;
    slab_slice_t* slab_slice = (slab_slice_t*)memptr;

    size_t page_size = SLAB_PAGE_2MB;
    size_t usable_page_size = page_size - slab_os_page_size - sizeof(slab_slice_t);
    size_t slab_slot_size = sizeof(slab_slot_t);
    uint32_t item_size = slab_allocator->object_size + slab_slot_size;
    uint32_t slots_count = (int)(usable_page_size / item_size);

    // Calculate how much space is needed at the beginning for the index and then
    // align the data address to the page size
    data_addr = memptr + (slab_slot_size * slots_count);
    data_addr = xalloc_mmap_align_addr((void*)data_addr);

    slab_slice->data.slab_allocator = slab_allocator;
    slab_slice->data.page_addr = memptr;
    slab_slice->data.data_addr = (uintptr_t)data_addr;
    slab_slice->data.metrics.objects_total_count = slots_count;
    slab_slice->data.metrics.objects_inuse_count = 0;
    slab_slice->data.available = true;

    return slab_slice;
}

void slab_allocator_slice_add_slots_to_per_core_metadata_slots(
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice,
        uint32_t core_index) {

    slab_slot_t* slab_slot;
    for(uint32_t index = 0; index < slab_slice->data.metrics.objects_total_count; index++) {
        slab_slot = &slab_slice->data.slots[index];
        slab_slot->data.available = true;
        slab_slot->data.memptr = (void*)(slab_slice->data.data_addr + (index * slab_allocator->object_size));

        double_linked_list_unshift_item(
                slab_allocator->core_metadata[core_index].slots,
                &slab_slot->double_linked_list_item);
    }
}

void slab_allocator_slice_remove_slots_from_per_core_metadata_slots(
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice,
        uint32_t core_index) {

    slab_slot_t* slab_slot;
    for(uint32_t index = 0; index < slab_slice->data.metrics.objects_total_count; index++) {
        slab_slot = &slab_slice->data.slots[index];
        double_linked_list_remove_item(
                slab_allocator->core_metadata[core_index].slots,
                &slab_slot->double_linked_list_item);
    }
}

slab_slice_t* slab_allocator_slice_from_memptr(
        void* memptr) {
    slab_slice_t* slab_slice = memptr - ((uintptr_t)memptr % SLAB_PAGE_2MB);
    return slab_slice;
}

void slab_allocator_slice_make_available(
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice,
        bool* can_free_slice) {
    *can_free_slice = false;
    slab_allocator_slice_remove_slots_from_per_core_metadata_slots(
            slab_allocator,
            slab_slice,
            current_thread_core_index);

    slab_allocator->core_metadata[current_thread_core_index].metrics.slices_total_count--;
    slab_allocator->core_metadata[current_thread_core_index].metrics.slices_free_count--;

    spinlock_section(&slab_allocator->spinlock, {
        if (slab_allocator->metrics.free_slices_count == 1) {
            *can_free_slice = true;

            slab_allocator->metrics.total_slices_count--;
            slab_allocator->numa_node_metadata[current_thread_numa_node_index].metrics.total_slices_count--;
            double_linked_list_remove_item(
                    slab_allocator->numa_node_metadata[current_thread_numa_node_index].slices,
                    &slab_slice->double_linked_list_item);
        } else {
            slab_allocator->metrics.free_slices_count++;
            slab_allocator->numa_node_metadata[current_thread_numa_node_index].metrics.free_slices_count++;
            slab_slice->data.available = true;
            double_linked_list_move_item_to_head(
                    slab_allocator->numa_node_metadata[current_thread_numa_node_index].slices,
                    &slab_slice->double_linked_list_item);
        }
    })
}

bool slab_allocator_slice_try_acquire(
        slab_allocator_t* slab_allocator) {
    bool slab_slice_found = false;
    double_linked_list_item_t* slab_slices_per_numa_node_head_item;
    slab_slice_t* head_slab_slice;

    // Check if an existing slices is available
    spinlock_section(&slab_allocator->spinlock, {
        slab_slices_per_numa_node_head_item =
                slab_allocator->numa_node_metadata[current_thread_numa_node_index].slices->head;
        head_slab_slice = (slab_slice_t *) slab_slices_per_numa_node_head_item;

        if (slab_slices_per_numa_node_head_item != NULL && head_slab_slice->data.available == true) {
            slab_slice_found = true;
            slab_allocator->metrics.free_slices_count--;
            slab_allocator->numa_node_metadata[current_thread_numa_node_index].metrics.free_slices_count--;
            head_slab_slice->data.available = false;
            double_linked_list_move_item_to_tail(
                    slab_allocator->numa_node_metadata[current_thread_numa_node_index].slices,
                    slab_slices_per_numa_node_head_item);
        }
    });

    if (slab_slice_found) {
        slab_allocator->core_metadata[current_thread_core_index].metrics.slices_inuse_count++;
        slab_allocator->core_metadata[current_thread_core_index].metrics.slices_total_count++;
        slab_allocator_slice_add_slots_to_per_core_metadata_slots(
                slab_allocator,
                head_slab_slice,
                current_thread_core_index);
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
        uint32_t numa_node_index,
        uint32_t core_index,
        void* memptr) {
    slab_allocator_ensure_core_index_and_numa_node_index_filled();

    // Initialize the new slice and set it to non available because it's going to be immediately used
    slab_slice_t* slab_slice = slab_allocator_slice_init(slab_allocator, memptr);
    slab_slice->data.available = false;

    // Add all the slots to the double linked list
    slab_allocator_slice_add_slots_to_per_core_metadata_slots(
            slab_allocator,
            slab_slice,
            core_index);
    slab_allocator->core_metadata[current_thread_core_index].metrics.slices_inuse_count++;
    slab_allocator->core_metadata[current_thread_core_index].metrics.slices_total_count++;

    // Add the slice to the ones initialized per core
    spinlock_section(&slab_allocator->spinlock, {
        slab_allocator->metrics.total_slices_count++;
        slab_allocator->numa_node_metadata[current_thread_numa_node_index].metrics.total_slices_count++;
        double_linked_list_push_item(
                slab_allocator->numa_node_metadata[numa_node_index].slices,
                &slab_slice->double_linked_list_item);
    })
}

void* slab_allocator_mem_alloc_hugepages(
        size_t size) {
    bool allocate_new_hugepage = false;
    slab_slot_t* slab_slot = NULL;
    slab_slice_t* slab_slice = NULL;

    slab_allocator_ensure_core_index_and_numa_node_index_filled();

    uint8_t predefined_slab_allocator_index = slab_index_by_object_size(size);
    slab_allocator_t* slab_allocator = predefined_slab_allocators[predefined_slab_allocator_index];

    slab_allocator_core_metadata_t* core_metadata = &slab_allocator->core_metadata[current_thread_core_index];

    spinlock_section(&core_metadata->spinlock, {
        double_linked_list_t* slots_per_core_list = core_metadata->slots;
        double_linked_list_item_t* slots_per_core_head_item = slots_per_core_list->head;
        slab_slot = (slab_slot_t*)slots_per_core_head_item;

        if (slots_per_core_head_item == NULL || slab_slot->data.available == false) {
            if (slab_allocator_slice_try_acquire(slab_allocator) == false) {
                if (!core_metadata->free_page_addr_first_inited) {
                    core_metadata->free_page_addr_first_inited = true;
                    core_metadata->free_page_addr = xalloc_hugepages_2mb_alloc(SLAB_PAGE_2MB);
                }

                // Ensure that free_page_addr is not null, if it is null the previous thread that used it hasn't
                // allocated the new one yet.
                // See comment below on ...if (core_metadata->free_page_addr == NULL)... for a bit more details on why
                // this look is necessary
                do {
                    HASHTABLE_MEMORY_FENCE_LOAD();
                } while (core_metadata->free_page_addr == NULL);

                slab_allocator_grow(
                        slab_allocator,
                        current_thread_numa_node_index,
                        current_thread_core_index,
                        core_metadata->free_page_addr);

                core_metadata->free_page_addr = NULL;
                allocate_new_hugepage = true;
            }

            slots_per_core_head_item = slots_per_core_list->head;
            slab_slot = (slab_slot_t*)slots_per_core_head_item;
        }

        slab_slot->data.available = false;
        double_linked_list_move_item_to_tail(slots_per_core_list, slots_per_core_head_item);

        slab_slice = slab_allocator_slice_from_memptr(slab_slot->data.memptr);
        slab_slice->data.metrics.objects_inuse_count++;

        core_metadata->metrics.objects_inuse_count++;
    });

    if (core_metadata->free_page_addr_first_inited == true && allocate_new_hugepage) {
        // Can be done without locking but with a memory write fence because the spinlock above guarantees that
        // only one thread can get through allocating a new huge page for the same core, if more than one thread
        // fetches the free hugepage then the read memory barrier and the check above will avoid using a null pointer.
        core_metadata->free_page_addr = xalloc_hugepages_2mb_alloc(SLAB_PAGE_2MB);
        HASHTABLE_MEMORY_FENCE_STORE();
    }

    return slab_slot->data.memptr;
}

void* slab_allocator_mem_alloc_zero(
        size_t size) {
    void* memptr = slab_allocator_mem_alloc(size);
    memset(memptr, 0, size);

    return memptr;
}

bool slab_allocator_mem_try_alloc(
        size_t size,
        void** memptr) {
    // TODO: need to implement a mechanism to check if the maximum allowed memory has been allocated or not
    //       counting the allocated slices
    *memptr = slab_allocator_mem_alloc(size);
    return true;
}

void slab_allocator_mem_free_hugepages(
        void* memptr) {
    assert(current_thread_core_index < UINT32_MAX);
    assert(current_thread_numa_node_index < UINT32_MAX);

    bool can_free_slab_slice = false;

    // Acquire the slab_slice, the slab_allocator, the slab_slot and the core_metadata for the current core on which
    // the thread is running
    slab_slice_t* slab_slice = slab_allocator_slice_from_memptr(memptr);
    slab_allocator_t* slab_allocator = slab_slice->data.slab_allocator;
    slab_slot_t* slot = slab_allocator_slot_from_memptr(
            slab_allocator,
            slab_slice,
            memptr);
    slab_allocator_core_metadata_t* core_metadata = &slab_allocator->core_metadata[current_thread_core_index];

    spinlock_section(&core_metadata->spinlock, {
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
                        &can_free_slab_slice);
            }
        }
    });

    if (can_free_slab_slice) {
        xalloc_hugepages_free(slab_slice->data.page_addr, SLAB_PAGE_2MB);
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
    if (slab_allocator_enabled) {
        return slab_allocator_mem_alloc_hugepages(size);
    } else {
        return slab_allocator_mem_alloc_xalloc(size);
    }
}

void slab_allocator_mem_free(
        void* memptr) {
    if (slab_allocator_enabled) {
        slab_allocator_mem_free_hugepages(memptr);
    } else {
        slab_allocator_mem_free_xalloc(memptr);
    }
}
