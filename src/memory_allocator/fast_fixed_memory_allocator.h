#ifndef CACHEGRAND_FAST_FIXED_MEMORY_ALLOCATOR_H
#define CACHEGRAND_FAST_FIXED_MEMORY_ALLOCATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FAST_FIXED_MEMORY_ALLOCATOR_DEBUG_ALLOCS_FREES
#define FAST_FIXED_MEMORY_ALLOCATOR_DEBUG_ALLOCS_FREES 0
#endif

#if FAST_FIXED_MEMORY_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
#warning "the fast fixed memory allocator built with allocs/frees debugging, will cause issues with valgrind and might hide bugs, use with caution!"
#endif

#define FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_16     0x00000010
#define FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_32     0x00000020
#define FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_64     0x00000040
#define FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_128    0x00000080
#define FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_256    0x00000100
#define FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_512    0x00000200
#define FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_1024   0x00000400
#define FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_2048   0x00000800
#define FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_4096   0x00001000
#define FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_8192   0x00002000
#define FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_16384  0x00004000
#define FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_32768  0x00008000
#define FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_65536  0x00010000

#define FAST_FIXED_MEMORY_ALLOCATOR_PREDEFINED_OBJECT_SIZES    FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_16, FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_32, FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_64, \
                                        FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_128, FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_256, FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_512, \
                                        FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_1024, FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_2048, FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_4096, \
                                        FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_8192, FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_16384, FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_32768, \
                                        FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_65536
#define FAST_FIXED_MEMORY_ALLOCATOR_PREDEFINED_OBJECT_SIZES_COUNT (sizeof(fast_fixed_memory_allocator_predefined_object_sizes) / sizeof(uint32_t))

#define FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_MIN    (((int[]){ FAST_FIXED_MEMORY_ALLOCATOR_PREDEFINED_OBJECT_SIZES })[0])
#define FAST_FIXED_MEMORY_ALLOCATOR_OBJECT_SIZE_MAX    (((int[]){ FAST_FIXED_MEMORY_ALLOCATOR_PREDEFINED_OBJECT_SIZES })[FAST_FIXED_MEMORY_ALLOCATOR_PREDEFINED_OBJECT_SIZES_COUNT - 1])

static const uint32_t fast_fixed_memory_allocator_predefined_object_sizes[] = { FAST_FIXED_MEMORY_ALLOCATOR_PREDEFINED_OBJECT_SIZES };

typedef struct fast_memory_allocator fast_fixed_memory_allocator_t;
struct fast_memory_allocator {
    // The slots and the slices are sorted per availability
    double_linked_list_t *slots;
    double_linked_list_t *slices;
    queue_mpmc_t *free_fast_fixed_memory_allocator_slots_queue_from_other_threads;

    // When the thread owning an instance of an allocator is terminated, other threads might still own some memory it
    // initialized and therefore some support is needed there.
    // When a thread sends back memory to a thread it has to check if it has been terminated and if yes, process the
    // free_fast_fixed_memory_allocator_slots_queue, free up the  slots, check if the slice owning the
    // fast_fixed_memory_allocator_slot is then empty, and in case return the hugepage.
    // All these operations have to be carried out under the external_thread_lock spinlock to avoid contention.
    bool_volatile_t fast_fixed_memory_allocator_freed;

    uint32_t object_size;

    struct {
        uint16_t slices_inuse_count;
        uint32_volatile_t objects_inuse_count;
    } metrics;

#if FAST_FIXED_MEMORY_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
    pid_t thread_id;
    char thread_name[101];
#endif
};

// It's necessary to use a union for fast_fixed_memory_allocator_slot_t and fast_fixed_memory_allocator_slice_t as the double_linked_list_item_t is being embedded
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
} fast_fixed_memory_allocator_slot_t;

typedef union {
    double_linked_list_item_t double_linked_list_item;
    struct {
        void *padding[2];
        fast_fixed_memory_allocator_t *fast_fixed_memory_allocator;
        void *page_addr;
        uintptr_t data_addr;
        bool available;
        struct {
            uint32_t objects_total_count;
            uint32_t objects_inuse_count;
        } metrics;
        fast_fixed_memory_allocator_slot_t slots[];
    } __attribute__((aligned(64))) data;
} fast_fixed_memory_allocator_slice_t;

#if FAST_FIXED_MEMORY_ALLOCATOR_DEBUG_ALLOCS_FREES == 1
void fast_fixed_memory_allocator_debug_allocs_frees_end();
#endif

fast_fixed_memory_allocator_t **fast_fixed_memory_allocator_thread_cache_init();

void fast_fixed_memory_allocator_thread_cache_free(
        void *data);

fast_fixed_memory_allocator_t** fast_fixed_memory_allocator_thread_cache_get();

void fast_fixed_memory_allocator_thread_cache_set(
        fast_fixed_memory_allocator_t** fast_fixed_memory_allocators);

bool fast_fixed_memory_allocator_thread_cache_has();

void fast_fixed_memory_allocator_enable(
        bool enable);

bool fast_fixed_memory_allocator_is_enabled();

fast_fixed_memory_allocator_t* fast_fixed_memory_allocator_thread_cache_get_fast_fixed_memory_allocator_by_size(
        size_t object_size);

fast_fixed_memory_allocator_t* fast_fixed_memory_allocator_init(
        size_t object_size);

bool fast_fixed_memory_allocator_free(
        fast_fixed_memory_allocator_t *fast_fixed_memory_allocator);

uint8_t fast_fixed_memory_allocator_index_by_object_size(
        size_t object_size);

size_t fast_fixed_memory_allocator_slice_calculate_usable_hugepage_size();

uint32_t fast_fixed_memory_allocator_slice_calculate_data_offset(
        size_t usable_hugepage_size,
        size_t object_size);

uint32_t fast_fixed_memory_allocator_slice_calculate_slots_count(
        size_t usable_hugepage_size,
        size_t data_offset,
        size_t object_size);

fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice_init(
        fast_fixed_memory_allocator_t *fast_fixed_memory_allocator,
        void *memptr);

void fast_fixed_memory_allocator_slice_add_slots_to_per_thread_metadata_slots(
        fast_fixed_memory_allocator_t *fast_fixed_memory_allocator,
        fast_fixed_memory_allocator_slice_t *fast_fixed_memory_allocator_slice);

void fast_fixed_memory_allocator_slice_remove_slots_from_per_thread_metadata_slots(
        fast_fixed_memory_allocator_t *fast_fixed_memory_allocator,
        fast_fixed_memory_allocator_slice_t *fast_fixed_memory_allocator_slice);

fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice_from_memptr(
        void *memptr);

void fast_fixed_memory_allocator_slice_make_available(
        fast_fixed_memory_allocator_t *fast_fixed_memory_allocator,
        fast_fixed_memory_allocator_slice_t *fast_fixed_memory_allocator_slice);

fast_fixed_memory_allocator_slot_t* fast_fixed_memory_allocator_slot_from_memptr(
        fast_fixed_memory_allocator_t *fast_fixed_memory_allocator,
        fast_fixed_memory_allocator_slice_t *fast_fixed_memory_allocator_slice,
        void *memptr);

void fast_fixed_memory_allocator_grow(
        fast_fixed_memory_allocator_t *fast_fixed_memory_allocator,
        void *memptr);

__attribute__((malloc))
void* fast_fixed_memory_allocator_mem_alloc_hugepages(
        fast_fixed_memory_allocator_t* fast_fixed_memory_allocator,
        size_t size);

void fast_fixed_memory_allocator_mem_free_hugepages_current_thread(
        fast_fixed_memory_allocator_t* fast_fixed_memory_allocator,
        fast_fixed_memory_allocator_slice_t* fast_fixed_memory_allocator_slice,
        fast_fixed_memory_allocator_slot_t* fast_fixed_memory_allocator_slot);

void fast_fixed_memory_allocator_mem_free_hugepages_different_thread(
        fast_fixed_memory_allocator_t* fast_fixed_memory_allocator,
        fast_fixed_memory_allocator_slot_t* fast_fixed_memory_allocator_slot);

void fast_fixed_memory_allocator_mem_free_hugepages(
        void *memptr);

__attribute__((malloc))
void* fast_fixed_memory_allocator_mem_alloc_xalloc(
        size_t size);

void fast_fixed_memory_allocator_mem_free_xalloc(
        void *memptr);

__attribute__((malloc))
void* fast_fixed_memory_allocator_mem_alloc(
        size_t size);

void* fast_fixed_memory_allocator_mem_alloc_zero(
        size_t size);

void* fast_fixed_memory_allocator_mem_realloc(
        void *memptr,
        size_t current_size,
        size_t new_size,
        bool zero_new_memory);

void fast_fixed_memory_allocator_mem_free(
        void *memptr);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FAST_FIXED_MEMORY_ALLOCATOR_H
