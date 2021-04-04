#ifndef CACHEGRAND_SLAB_ALLOCATOR_H
#define CACHEGRAND_SLAB_ALLOCATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#define SLAB_OBJECT_SIZE_64     0x0040
#define SLAB_OBJECT_SIZE_128    0x0080
#define SLAB_OBJECT_SIZE_256    0x0100
#define SLAB_OBJECT_SIZE_512    0x0200
#define SLAB_OBJECT_SIZE_1024   0x0400
#define SLAB_OBJECT_SIZE_2048   0x0800
#define SLAB_OBJECT_SIZE_4096   0x1000
#define SLAB_OBJECT_SIZE_8192   0x2000
#define SLAB_OBJECT_SIZE_16384  0x4000
#define SLAB_OBJECT_SIZE_32768  0x8000
#define SLAB_OBJECT_SIZE_MAX    SLAB_OBJECT_SIZE_32768

#define SLAB_OBJECT_SIZES       SLAB_OBJECT_SIZE_64, SLAB_OBJECT_SIZE_128, SLAB_OBJECT_SIZE_256, SLAB_OBJECT_SIZE_512, \
                                SLAB_OBJECT_SIZE_1024, SLAB_OBJECT_SIZE_2048, SLAB_OBJECT_SIZE_4096, \
                                SLAB_OBJECT_SIZE_8192, SLAB_OBJECT_SIZE_16384, SLAB_OBJECT_SIZE_32768
#define SLAB_OBJECT_SIZES_COUNT 10


typedef struct slab_allocator slab_allocator_t;
struct slab_allocator {
    spinlock_lock_volatile_t spinlock;
    uint16_t core_count;
    uint16_t numa_node_count;
    uint16_t object_size;
    uint32_t total_slices_count;
    uint32_t free_slices_count;
    double_linked_list_t** slices_per_numa_node;

    // The slots are sorted per availability
    double_linked_list_t** slots_per_core;
};

// It's necessary to use an union for slab_slot_t and slab_slice_t as the double_linked_list_item_t is being embedded
// to avoid allocating an empty pointer to data wasting 8 bytes.
// Currently double_linked_list_item_t contains 3 pointers, prev, next and data, so a void* padding[2] is necessary to
// do not overwrite prev and next, if the struct behind double_linked_list_item_t changes it's necessary to update the
// data structures below.

typedef union {
    double_linked_list_item_t double_linked_list_item;
    struct {
        void* padding[2];
        void* memptr;
        bool available;
    } data;
} slab_slot_t;

typedef union {
    double_linked_list_item_t double_linked_list_item;
    struct {
        void* padding[2];
        slab_allocator_t* slab_allocator;
        void* page_addr;
        uintptr_t data_addr;
        uint32_t count;
        uint32_t used;
        bool available;
        slab_slot_t slots[];
    } __attribute__((aligned(64))) data;
} slab_slice_t;

slab_allocator_t* slab_allocator_predefined_get_by_size(
        size_t object_size);

uint32_t slab_allocator_get_current_thread_numa_node_index();

uint32_t slab_allocator_get_current_thread_core_index();

slab_allocator_t* slab_allocator_init(
        size_t object_size);

void slab_allocator_free(
        slab_allocator_t* slab);

void slab_allocator_ensure_core_index_and_numa_node_index_filled();

uint8_t slab_index_by_object_size(
        size_t object_size);

slab_slice_t* slab_allocator_slice_init(
        slab_allocator_t* slab_allocator,
        void* memptr);

void slab_allocator_slice_add_slots_to_slots_per_core(
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice,
        uint32_t core_index);

void slab_allocator_slice_remove_slots_from_slots_per_core(
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice,
        uint32_t core_index);

void slab_allocator_slice_make_available(
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice);

bool slab_allocator_slice_try_acquire(
        slab_allocator_t* slab_allocator);

void slab_allocator_grow(
        slab_allocator_t* slab_allocator,
        uint32_t numa_node_index,
        uint32_t core_index,
        void* memptr);

void slab_allocator_hugepage_free(
        void* memptr,
        size_t size);

void* slab_allocator_mem_alloc(
        size_t size);

bool slab_allocator_mem_try_alloc(
        size_t size,
        void** memptr);

void slab_allocator_mem_free(
        void* memptr);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_SLAB_ALLOCATOR_H
