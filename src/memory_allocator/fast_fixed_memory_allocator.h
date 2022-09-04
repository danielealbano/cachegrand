#ifndef CACHEGRAND_FAST_MEMORY_ALLOCATOR_H
#define CACHEGRAND_FAST_MEMORY_ALLOCATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SLAB_ALLOCATOR_DEBUG_ALLOCS_FREES
#define SLAB_ALLOCATOR_DEBUG_ALLOCS_FREES 0
#endif

#if SLAB_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
#warning "slab allocator built with allocs/frees debugging, will cause issues with valgrind and might hide bugs, use with caution!"
#endif

#define SLAB_OBJECT_SIZE_16     0x00000010
#define SLAB_OBJECT_SIZE_32     0x00000020
#define SLAB_OBJECT_SIZE_64     0x00000040
#define SLAB_OBJECT_SIZE_128    0x00000080
#define SLAB_OBJECT_SIZE_256    0x00000100
#define SLAB_OBJECT_SIZE_512    0x00000200
#define SLAB_OBJECT_SIZE_1024   0x00000400
#define SLAB_OBJECT_SIZE_2048   0x00000800
#define SLAB_OBJECT_SIZE_4096   0x00001000
#define SLAB_OBJECT_SIZE_8192   0x00002000
#define SLAB_OBJECT_SIZE_16384  0x00004000
#define SLAB_OBJECT_SIZE_32768  0x00008000
#define SLAB_OBJECT_SIZE_65536  0x00010000

#define SLAB_PREDEFINED_OBJECT_SIZES    SLAB_OBJECT_SIZE_16, SLAB_OBJECT_SIZE_32, SLAB_OBJECT_SIZE_64, \
                                        SLAB_OBJECT_SIZE_128, SLAB_OBJECT_SIZE_256, SLAB_OBJECT_SIZE_512, \
                                        SLAB_OBJECT_SIZE_1024, SLAB_OBJECT_SIZE_2048, SLAB_OBJECT_SIZE_4096, \
                                        SLAB_OBJECT_SIZE_8192, SLAB_OBJECT_SIZE_16384, SLAB_OBJECT_SIZE_32768, \
                                        SLAB_OBJECT_SIZE_65536
#define SLAB_PREDEFINED_OBJECT_SIZES_COUNT (sizeof(slab_predefined_object_sizes) / sizeof(uint32_t))

#define SLAB_OBJECT_SIZE_MIN    (((int[]){ SLAB_PREDEFINED_OBJECT_SIZES })[0])
#define SLAB_OBJECT_SIZE_MAX    (((int[]){ SLAB_PREDEFINED_OBJECT_SIZES })[SLAB_PREDEFINED_OBJECT_SIZES_COUNT - 1])

static const uint32_t slab_predefined_object_sizes[] = { SLAB_PREDEFINED_OBJECT_SIZES };

typedef struct fast_memory_allocator slab_allocator_t;
struct fast_memory_allocator {
    // The slots and the slices are sorted per availability
    double_linked_list_t *slots;
    double_linked_list_t *slices;
    queue_mpmc_t *free_slab_slots_queue_from_other_threads;

    // When the thread owning a slab allocator is terminated, other threads might still own some memory it initialized
    // and therefore some support is needed there.
    // When a thread sends back memory to a thread it has to check if it has been terminated and if yes, process the
    // free_slab_slots_queue, free up the slab slots, check if the slice owning the slab_slot is then empty, and in case
    // return the hugepage.
    // All these operations have to be carried out under the external_thread_lock spinlock to avoid contention.
    bool_volatile_t slab_allocator_freed;

    uint32_t object_size;

    struct {
        uint16_t slices_inuse_count;
        uint32_volatile_t objects_inuse_count;
    } metrics;

#if SLAB_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
    pid_t thread_id;
    char thread_name[101];
#endif
};

// It's necessary to use a union for slab_slot_t and slab_slice_t as the double_linked_list_item_t is being embedded
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
} slab_slot_t;

typedef union {
    double_linked_list_item_t double_linked_list_item;
    struct {
        void *padding[2];
        slab_allocator_t *slab_allocator;
        void *page_addr;
        uintptr_t data_addr;
        bool available;
        struct {
            uint32_t objects_total_count;
            uint32_t objects_inuse_count;
        } metrics;
        slab_slot_t slots[];
    } __attribute__((aligned(64))) data;
} slab_slice_t;

#if SLAB_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
void slab_allocator_debug_allocs_frees_end();
#endif

slab_allocator_t **slab_allocator_thread_cache_init();

void slab_allocator_thread_cache_free(
        void *data);

slab_allocator_t** slab_allocator_thread_cache_get();

void slab_allocator_thread_cache_set(
        slab_allocator_t** slab_allocators);

bool slab_allocator_thread_cache_has();

void slab_allocator_enable(
        bool enable);

bool slab_allocator_is_enabled();

slab_allocator_t* slab_allocator_thread_cache_get_slab_allocator_by_size(
        size_t object_size);

slab_allocator_t* slab_allocator_init(
        size_t object_size);

bool slab_allocator_free(
        slab_allocator_t *slab);

uint8_t slab_index_by_object_size(
        size_t object_size);

size_t slab_allocator_slice_calculate_usable_hugepage_size();

uint32_t slab_allocator_slice_calculate_data_offset(
        size_t usable_hugepage_size,
        size_t object_size);

uint32_t slab_allocator_slice_calculate_slots_count(
        size_t usable_hugepage_size,
        size_t data_offset,
        size_t object_size);

slab_slice_t* slab_allocator_slice_init(
        slab_allocator_t *slab_allocator,
        void *memptr);

void slab_allocator_slice_add_slots_to_per_thread_metadata_slots(
        slab_allocator_t *slab_allocator,
        slab_slice_t *slab_slice);

void slab_allocator_slice_remove_slots_from_per_thread_metadata_slots(
        slab_allocator_t *slab_allocator,
        slab_slice_t *slab_slice);

slab_slice_t* slab_allocator_slice_from_memptr(
        void *memptr);

void slab_allocator_slice_make_available(
        slab_allocator_t *slab_allocator,
        slab_slice_t *slab_slice);

slab_slot_t* slab_allocator_slot_from_memptr(
        slab_allocator_t *slab_allocator,
        slab_slice_t *slab_slice,
        void *memptr);

void slab_allocator_grow(
        slab_allocator_t *slab_allocator,
        void *memptr);

__attribute__((malloc))
void* slab_allocator_mem_alloc_hugepages(
        slab_allocator_t* slab_allocator,
        size_t size);

void slab_allocator_mem_free_hugepages_current_thread(
        slab_allocator_t* slab_allocator,
        slab_slice_t* slab_slice,
        slab_slot_t* slab_slot);

void slab_allocator_mem_free_hugepages_different_thread(
        slab_allocator_t* slab_allocator,
        slab_slot_t* slab_slot);

void slab_allocator_mem_free_hugepages(
        void *memptr);

__attribute__((malloc))
void* slab_allocator_mem_alloc_xalloc(
        size_t size);

void slab_allocator_mem_free_xalloc(
        void *memptr);

__attribute__((malloc))
void* slab_allocator_mem_alloc(
        size_t size);

void* slab_allocator_mem_alloc_zero(
        size_t size);

void* slab_allocator_mem_realloc(
        void *memptr,
        size_t current_size,
        size_t new_size,
        bool zero_new_memory);

void slab_allocator_mem_free(
        void *memptr);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FAST_MEMORY_ALLOCATOR_H
