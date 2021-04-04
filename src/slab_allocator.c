#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sched.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "xalloc.h"
#include "fatal.h"
#include "utils_cpu.h"
#include "utils_numa.h"
#include "data_structures/double_linked_list/double_linked_list.h"

#include "slab_allocator.h"

#define TAG "slab_allocator"

/**
 * The slab allocator HEAVILY relies on the hugepages, the hugepage address is 2MB aligned therefore it's possible to
 * calculate the initial address of the page and place the index of slab slice at the beginning and use the rest of the
 * page to store the data.
 *
 * The logic should be improved to
 */

#define SLAB_PAGE_2MB   (2 * 1024 * 1024)

thread_local uint32_t current_thread_core_index = UINT32_MAX;
thread_local uint32_t current_thread_numa_node_index = UINT32_MAX;

size_t slab_os_page_size;
slab_allocator_t* predefined_slab_allocators[SLAB_OBJECT_SIZES_COUNT];

FUNCTION_CTOR(slab_allocator_init, {
    // Get page size
    slab_os_page_size = xalloc_get_page_size();

    // Allocates all the slab allocator per object size (can't do an array, the preprocessor gets angry because of the
    // commas)
    predefined_slab_allocators[0] = slab_allocator_init(SLAB_OBJECT_SIZE_64);
    predefined_slab_allocators[1] = slab_allocator_init(SLAB_OBJECT_SIZE_128);
    predefined_slab_allocators[2] = slab_allocator_init(SLAB_OBJECT_SIZE_256);
    predefined_slab_allocators[3] = slab_allocator_init(SLAB_OBJECT_SIZE_512);
    predefined_slab_allocators[4] = slab_allocator_init(SLAB_OBJECT_SIZE_1024);
    predefined_slab_allocators[5] = slab_allocator_init(SLAB_OBJECT_SIZE_2048);
    predefined_slab_allocators[6] = slab_allocator_init(SLAB_OBJECT_SIZE_4096);
    predefined_slab_allocators[7] = slab_allocator_init(SLAB_OBJECT_SIZE_8192);
    predefined_slab_allocators[8] = slab_allocator_init(SLAB_OBJECT_SIZE_16384);
    predefined_slab_allocators[9] = slab_allocator_init(SLAB_OBJECT_SIZE_32768);
})

void slab_allocator_ensure_core_index_and_numa_node_index_filled() {
    if (current_thread_core_index == UINT32_MAX || current_thread_numa_node_index == UINT32_MAX) {
        getcpu(&current_thread_core_index, &current_thread_numa_node_index);
    }
}

uint8_t slab_index_by_object_size(
        size_t object_size) {
    assert(object_size <= SLAB_OBJECT_SIZE_32768);

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
    slab_allocator->slices_per_numa = xalloc_alloc_zero(sizeof(double_linked_list_t) * numa_node_count);
    slab_allocator->slots_per_core = xalloc_alloc_zero(sizeof(double_linked_list_t) * core_count);
    slab_allocator->metrics_per_core = xalloc_alloc_zero(sizeof(slab_allocator_metrics_per_core_t) * core_count);
    slab_allocator->slices_count = 0;

    for(int i = 0; i < slab_allocator->core_count; i++) {
        slab_allocator->slots_per_core[i] = double_linked_list_init();
    }

    for(int i = 0; i < slab_allocator->numa_node_count; i++) {
        slab_allocator->slices_per_numa[i] = double_linked_list_init();
    }

    return slab_allocator;
}

void slab_allocator_free(
        slab_allocator_t* slab_allocator) {

    for(int i = 0; i < slab_allocator->numa_node_count; i++) {
        double_linked_list_item_t* item = NULL;
        while((item = double_linked_list_iter_next(slab_allocator->slices_per_numa[i], item)) != NULL) {
            slab_slice_t* slab_slice = item->data;
            xalloc_hugepages_free(slab_slice->page_addr, SLAB_PAGE_2MB);
        }
    }

    for(int i = 0; i < slab_allocator->core_count; i++) {
        double_linked_list_free(slab_allocator->slots_per_core[i]);
    }

    for(int i = 0; i < slab_allocator->numa_node_count; i++) {
        double_linked_list_free(slab_allocator->slices_per_numa[i]);
    }

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

    slab_slice->slab_allocator = slab_allocator;
    slab_slice->page_addr = memptr;
    slab_slice->data_addr = (uintptr_t)data_addr;
    slab_slice->count = slots_count;

    return slab_slice;
}

void slab_allocator_slice_add_slots_to_slots_per_core(
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice,
        uint32_t core_index) {

    slab_slot_t* slab_slot;
    for(uint32_t index = 0; index < slab_slice->count; index++) {
        slab_slot = &slab_slice->slots[index];
        slab_slot->data.available = true;
        slab_slot->data.memptr = (void*)(slab_slice->data_addr + (index * slab_allocator->object_size));

        double_linked_list_unshift_item(slab_allocator->slots_per_core[core_index], &slab_slot->item);
    }
}

void slab_allocator_grow(
        slab_allocator_t* slab_allocator,
        uint32_t numa_node_index,
        uint32_t core_index,
        void* memptr) {
    // Initialize the new slice
    slab_slice_t* slab_slice = slab_allocator_slice_init(slab_allocator, memptr);

    // Add all the slots to the double linked list
    slab_allocator_slice_add_slots_to_slots_per_core(
            slab_allocator,
            slab_slice,
            core_index);

    // Add the slice to the ones initialized per core
    double_linked_list_item_t* slab_slice_item = double_linked_list_item_init();
    slab_slice_item->data = slab_slice;
    double_linked_list_push_item(slab_allocator->slices_per_numa[numa_node_index], slab_slice_item);
    slab_allocator->slices_count++;
}

void* slab_allocator_mem_alloc(
        size_t size) {
    slab_allocator_ensure_core_index_and_numa_node_index_filled();

    uint8_t predefined_slab_allocator_index = slab_index_by_object_size(size);
    slab_allocator_t* slab_allocator = predefined_slab_allocators[predefined_slab_allocator_index];

    double_linked_list_t* list = slab_allocator->slots_per_core[current_thread_core_index];
    double_linked_list_item_t* item = list->head;
    slab_slot_t* slab_slot = (slab_slot_t*)item;

    if (item == NULL || slab_slot->data.available == false) {
        void* memptr = xalloc_hugepages_2mb_alloc(SLAB_PAGE_2MB);
        slab_allocator_grow(
                slab_allocator,
                current_thread_numa_node_index,
                current_thread_core_index,
                memptr);

        item = list->head;
        slab_slot = (slab_slot_t*)item;
    }

    slab_slot->data.available = false;
    double_linked_list_move_item_to_tail(list, item);

    // TODO: update metrics

    return slab_slot->data.memptr;
}

bool slab_allocator_mem_try_alloc(
        size_t size,
        void** memptr) {
    // TODO: need to implement a mechanism to check if the maximum allowed memory has been allocated or not
    //       counting the allocated slices
    *memptr = slab_allocator_mem_alloc(size);
    return true;
}

void slab_allocator_mem_free(
        void* memptr) {
    slab_allocator_ensure_core_index_and_numa_node_index_filled();

    slab_slice_t* slab_slice = memptr - ((uintptr_t)memptr % SLAB_PAGE_2MB);

    slab_allocator_t* slab_allocator = slab_slice->slab_allocator;
    double_linked_list_t* slots_list = slab_allocator->slots_per_core[current_thread_core_index];

    assert((((uintptr_t)memptr - (uintptr_t)slab_slice->data_addr) % slab_allocator->object_size) == 0);

    uint16_t object_index = ((uintptr_t)memptr - (uintptr_t)slab_slice->data_addr) / slab_allocator->object_size;
    slab_slot_t* slot = &slab_slice->slots[object_index];

    slot->data.available = true;
    double_linked_list_move_item_to_head(slots_list, &slot->item);

    // TODO: update metrics
}
