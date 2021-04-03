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


typedef struct slab_allocator_metrics_per_core slab_allocator_metrics_per_core_t;
struct slab_allocator_metrics_per_core {
    uint32_t total;
    uint32_t used;
};

typedef struct slab_allocator slab_allocator_t;
struct slab_allocator {
    spinlock_lock_volatile_t spinlock;
    uint16_t core_count;
    uint16_t numa_node_count;
    uint16_t object_size;
    uint32_t slices_count;
    slab_allocator_metrics_per_core_t** metrics_per_core;
    double_linked_list_t** slices_per_numa;

    // The slots are sorted per availability
    double_linked_list_t** slots_per_core;
};

// The padding of the struct data is needed to to avoid overwriting the prev/next of the double linked list item, any
// change done to the double_linked_list_item_t structure has to be reflected here. Also it's necessary to avoid useless
// growth because it would mean occupying much more memory and not being aligned with a cacheline (both are verified
// within the tests)
typedef union {
    double_linked_list_item_t item;
    struct {
        void* padding[2];
        void* memptr;
        bool available;
    } data;
} slab_slot_t;

typedef struct slab_slice slab_slice_t;
struct slab_slice {
    slab_allocator_t* slab_allocator;
    void* page_addr;
    uintptr_t data_addr;
    uint32_t count;
    slab_slot_t slots[];
};

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

void slab_allocator_grow(
        slab_allocator_t* slab_allocator,
        uint32_t numa_node_index,
        uint32_t core_index,
        void* memptr);

void* slab_allocator_hugepage_alloc(
        size_t size);

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
