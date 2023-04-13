#ifndef CACHEGRAND_FFMA_H
#define CACHEGRAND_FFMA_H

// For optimization purposes a number of functions are in this header as static inline and they need certain headers,
// so we need to include them here to avoid having to include them in every file that includes this header.
#ifdef __cplusplus
#include <cstring>
#include <cassert>
#else
#include <string.h>
#include <assert.h>
#endif

#include "misc.h"
#include "clock.h"
#include "log/log.h"
#include "ffma_region_cache.h"
#include "ffma_thread_cache.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FFMA_LOG_TAG_INTERNAL "ffma"

#ifndef FFMA_DEBUG_ALLOCS_FREES
#define FFMA_DEBUG_ALLOCS_FREES 0
#endif

#define FFMA_PREINIT_SOME_SLOTS_COUNT (16)

#define FFMA_SLICE_SIZE         (8 * 1024 * 1024)
#define FFMA_REGION_CACHE_SIZE  (32)

#define FFMA_OBJECT_SIZE_16     (0x00000010)
#define FFMA_OBJECT_SIZE_32     (0x00000020)
#define FFMA_OBJECT_SIZE_64     (0x00000040)
#define FFMA_OBJECT_SIZE_128    (0x00000080)
#define FFMA_OBJECT_SIZE_256    (0x00000100)
#define FFMA_OBJECT_SIZE_512    (0x00000200)
#define FFMA_OBJECT_SIZE_1024   (0x00000400)
#define FFMA_OBJECT_SIZE_2048   (0x00000800)
#define FFMA_OBJECT_SIZE_4096   (0x00001000)
#define FFMA_OBJECT_SIZE_8192   (0x00002000)
#define FFMA_OBJECT_SIZE_16384  (0x00004000)
#define FFMA_OBJECT_SIZE_32768  (0x00008000)
#define FFMA_OBJECT_SIZE_65536  (0x00010000)

#define FFMA_PREDEFINED_OBJECT_SIZES    FFMA_OBJECT_SIZE_16, FFMA_OBJECT_SIZE_32, FFMA_OBJECT_SIZE_64, \
                                        FFMA_OBJECT_SIZE_128, FFMA_OBJECT_SIZE_256, FFMA_OBJECT_SIZE_512, \
                                        FFMA_OBJECT_SIZE_1024, FFMA_OBJECT_SIZE_2048, FFMA_OBJECT_SIZE_4096, \
                                        FFMA_OBJECT_SIZE_8192, FFMA_OBJECT_SIZE_16384, FFMA_OBJECT_SIZE_32768, \
                                        FFMA_OBJECT_SIZE_65536
#define FFMA_PREDEFINED_OBJECT_SIZES_COUNT (sizeof(ffma_predefined_object_sizes) / sizeof(uint32_t))

#define FFMA_OBJECT_SIZE_MIN    (((int[]){ FFMA_PREDEFINED_OBJECT_SIZES })[0])
#define FFMA_OBJECT_SIZE_MAX    (((int[]){ FFMA_PREDEFINED_OBJECT_SIZES })[FFMA_PREDEFINED_OBJECT_SIZES_COUNT - 1])

static const uint32_t ffma_predefined_object_sizes[] = { FFMA_PREDEFINED_OBJECT_SIZES };

extern ffma_region_cache_t *internal_ffma_region_cache;

typedef struct ffma ffma_t;
struct ffma {
    // The slots and the slices are sorted per availability
    double_linked_list_t *slots;
    double_linked_list_t *slices;
    queue_mpmc_t free_ffma_slots_queue_from_other_threads;

    // When the thread owning an instance of an allocator is terminated, other threads might still own some memory it
    // initialized and therefore some support is needed there.
    // When a thread sends back memory to a thread it has to check if it has been terminated and if yes, process the
    // free_ffma_slots_queue, free up the  slots, check if the slice owning the
    // ffma_slot is then empty, and in case return the address sof the page.
    bool_volatile_t ffma_freed;

    uint32_t object_size;

    struct {
        uint16_t slices_inuse_count;
        uint32_volatile_t objects_inuse_count;
    } metrics;

#if FFMA_DEBUG_ALLOCS_FREES == 1
    pid_t thread_id;
    char thread_name[101];
#endif
};

// It's necessary to use a union for ffma_slot_t and ffma_slice_t as the double_linked_list_item_t is being embedded
// to avoid allocating an empty pointer to data wasting 8 bytes.
// Currently, double_linked_list_item_t contains 3 pointers, prev, next and data, so a void* padding[2] is necessary to
// do not overwrite prev and next, if the struct behind double_linked_list_item_t changes it's necessary to update the
// data structures below.

typedef union {
    double_linked_list_item_t double_linked_list_item;
    struct {
        void *padding[2];
        void *memptr;
#if DEBUG==1
        bool available:1;
        int32_t allocs:31;
        int32_t frees:31;
#else
        bool available;
#endif
    } data;
} ffma_slot_t;

typedef union {
    double_linked_list_item_t double_linked_list_item;
    struct {
        void *padding[2];
        ffma_t *ffma;
        void *page_addr;
        uintptr_t data_addr;
        bool available;
        struct {
            uint32_t objects_total_count;
            uint32_t objects_initialized_count;
            uint32_t objects_inuse_count;
        } metrics;
        ffma_slot_t slots[];
    } __attribute__((aligned(64))) data;
} ffma_slice_t;

#if FFMA_DEBUG_ALLOCS_FREES == 1
void ffma_debug_allocs_frees_end();
#endif

void ffma_set_use_hugepages(
        bool use_hugepages);

ffma_t* ffma_init(
        size_t object_size);

bool ffma_free(
        ffma_t *ffma);

ffma_slice_t* ffma_slice_init(
        ffma_t *ffma,
        void *memptr);

void ffma_slice_add_slots_to_per_thread_metadata_slots(
        ffma_t *ffma,
        ffma_slice_t *ffma_slice);

void ffma_slice_remove_slots_from_per_thread_metadata_slots(
        ffma_t *ffma,
        ffma_slice_t *ffma_slice);

void ffma_slice_make_available(
        ffma_t *ffma,
        ffma_slice_t *ffma_slice);

void ffma_grow(
        ffma_t *ffma,
        void *memptr);

void ffma_mem_free_slot_in_current_thread(
        ffma_t* ffma,
        ffma_slice_t* ffma_slice,
        ffma_slot_t* ffma_slot);

void ffma_mem_free_slot_different_thread(
        ffma_t* ffma,
        ffma_slot_t* ffma_slot);

void* ffma_mem_realloc(
        void *memptr,
        size_t current_size,
        size_t new_size,
        bool zero_new_memory);

void ffma_mem_free(
        void *memptr);

static inline uint8_t ffma_index_by_object_size(
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

static inline ffma_slice_t* ffma_slice_from_memptr(
        void* memptr) {
    uintptr_t memptr_uintptr = (uintptr_t)memptr;
    ffma_slice_t* ffma_slice = (ffma_slice_t*)(memptr_uintptr - (memptr_uintptr % FFMA_SLICE_SIZE));

    return ffma_slice;
}

static inline size_t ffma_slice_calculate_usable_memory_size(
        size_t ffma_os_page_size) {
    size_t usable_memory_size = FFMA_SLICE_SIZE - ffma_os_page_size - sizeof(ffma_slice_t);

    return usable_memory_size;
}

static inline uint32_t ffma_slice_calculate_data_offset(
        size_t ffma_os_page_size,
        size_t usable_memory_size,
        size_t object_size) {
    uint32_t slots_count = (int)(usable_memory_size / (object_size + sizeof(ffma_slot_t)));
    size_t data_offset = sizeof(ffma_slice_t) + (slots_count * sizeof(ffma_slot_t));
    data_offset += ffma_os_page_size - (data_offset % ffma_os_page_size);

    return data_offset;
}

static inline uint32_t ffma_slice_calculate_slots_count(
        size_t usable_memory_size,
        size_t data_offset,
        size_t object_size) {
    size_t data_size = usable_memory_size - data_offset;
    uint32_t slots_count = data_size / object_size;

    return slots_count;
}

static inline ffma_slot_t* ffma_slot_from_memptr(
        ffma_t* ffma,
        ffma_slice_t* ffma_slice,
        void* memptr) {
    uint32_t object_index = ((uintptr_t)memptr - (uintptr_t)ffma_slice->data.data_addr) / ffma->object_size;
    ffma_slot_t* slot = &ffma_slice->data.slots[object_index];

    return slot;
}

static inline ffma_t* ffma_thread_cache_get_ffma_by_size(
        size_t object_size) {
    ffma_t **ffma_list = ffma_thread_cache_get();

    return ffma_list[ffma_index_by_object_size(object_size)];
}

static inline bool ffma_slice_can_add_one_slot_to_per_thread_metadata_slots(
        ffma_slice_t* ffma_slice) {
    return ffma_slice->data.metrics.objects_initialized_count < ffma_slice->data.metrics.objects_total_count;
}

static inline void ffma_slice_add_some_slots_to_per_thread_metadata_slots(
        ffma_t* ffma,
        ffma_slice_t* ffma_slice) {
    ffma_slot_t* ffma_slot;

    uint32_t index_end = ffma_slice->data.metrics.objects_initialized_count + FFMA_PREINIT_SOME_SLOTS_COUNT;
    if (unlikely(index_end > ffma_slice->data.metrics.objects_total_count)) {
        index_end = ffma_slice->data.metrics.objects_total_count;
    }

#pragma unroll(FFMA_PREINIT_SOME_SLOTS_COUNT)
    for(uint32_t index = ffma_slice->data.metrics.objects_initialized_count; index < index_end; index++) {
        assert(index < ffma_slice->data.metrics.objects_total_count);
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

__attribute__((malloc))
static inline void* ffma_mem_alloc_internal(
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

    if (unlikely(ffma_slot == NULL || ffma_slot->data.available == false)) {
        // If the local cache is empty tries to check if the slices has slot to initialize and if yes initializes one
        // and adds it to the local cache.
        // The newly added slice with uninitialized slots is added to the tail of the list during the growing process so
        // it's possible to check the tail of the slices list.
        if (likely(
                ffma->slices->tail != NULL &&
                ffma_slice_can_add_one_slot_to_per_thread_metadata_slots((ffma_slice_t*)ffma->slices->tail))) {
            ffma_slice_add_some_slots_to_per_thread_metadata_slots(ffma, (ffma_slice_t*)ffma->slices->tail);

            // After adding the slot to the local cache tries to get it again
            slots_head_item = slots_list->head;
            ffma_slot = (ffma_slot_t *) slots_head_item;

            // There should always be a valid slot so if it's not the case it's a bug
            assert(ffma_slot != NULL);
            assert(ffma_slot->data.available == true);

            // If it can't get the slot from the local cache tries to fetch if from the free list which is a bit slower as
            // it involves atomic operations, on the other end it requires less operation to be prepared as e.g. it is
            // already on the correct side of the slots double linked list
        } else if ((ffma_slot =
                (ffma_slot_t *) queue_mpmc_pop(&ffma->free_ffma_slots_queue_from_other_threads)) != NULL) {
            assert(ffma_slot->data.memptr != NULL);
            ffma_slot->data.available = false;

#if DEBUG == 1
            ffma_slot->data.allocs++;

#if defined(HAS_VALGRIND)
            ffma_slice = ffma_slice_from_memptr(ffma_slot->data.memptr);
            VALGRIND_MEMPOOL_ALLOC(ffma_slice->data.page_addr, ffma_slot->data.memptr, size);
#endif
#endif

            // To keep the code simpler and avoid convoluted ifs, the code returns here
            return ffma_slot->data.memptr;
        }

        if (ffma_slot == NULL || ffma_slot->data.available == false) {
            void *addr = ffma_region_cache_pop(internal_ffma_region_cache);

            if (!addr) {
                LOG_E(FFMA_LOG_TAG_INTERNAL, "Unable to allocate %lu bytes of memory", size);
                return NULL;
            }

            ffma_grow(
                    ffma,
                    addr);

            slots_head_item = slots_list->head;
            ffma_slot = (ffma_slot_t *) slots_head_item;
        }
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


__attribute__((malloc))
static inline void* ffma_mem_alloc(
        size_t size) {
    void* memptr;

    assert(size > 0);

    if (unlikely(!ffma_thread_cache_has())) {
        ffma_thread_cache_set(ffma_thread_cache_init());
    }

    uint64_t index = ffma_index_by_object_size(size);
    assert(index < FFMA_PREDEFINED_OBJECT_SIZES_COUNT);

    ffma_t *ffma = ffma_thread_cache_get()[index];
    memptr = ffma_mem_alloc_internal(ffma, size);

    assert(memptr != NULL);

    return memptr;
}

static inline void* ffma_mem_alloc_zero(
        size_t size) {
    void* memptr = ffma_mem_alloc(size);
    if (memptr) {
        memset(memptr, 0, size);
    }

    return memptr;
}

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FFMA_H
