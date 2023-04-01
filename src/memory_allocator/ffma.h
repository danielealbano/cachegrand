#ifndef CACHEGRAND_FFMA_H
#define CACHEGRAND_FFMA_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FFMA_DEBUG_ALLOCS_FREES
#define FFMA_DEBUG_ALLOCS_FREES 0
#endif

#if FFMA_DEBUG_ALLOCS_FREES == 1
#warning "the fast fixed memory allocator built with allocs/frees debugging, will cause issues with valgrind and might hide bugs, use with caution!"
#endif

#define FFMA_OBJECT_SIZE_16     0x00000010
#define FFMA_OBJECT_SIZE_32     0x00000020
#define FFMA_OBJECT_SIZE_64     0x00000040
#define FFMA_OBJECT_SIZE_128    0x00000080
#define FFMA_OBJECT_SIZE_256    0x00000100
#define FFMA_OBJECT_SIZE_512    0x00000200
#define FFMA_OBJECT_SIZE_1024   0x00000400
#define FFMA_OBJECT_SIZE_2048   0x00000800
#define FFMA_OBJECT_SIZE_4096   0x00001000
#define FFMA_OBJECT_SIZE_8192   0x00002000
#define FFMA_OBJECT_SIZE_16384  0x00004000
#define FFMA_OBJECT_SIZE_32768  0x00008000
#define FFMA_OBJECT_SIZE_65536  0x00010000

#define FFMA_PREDEFINED_OBJECT_SIZES    FFMA_OBJECT_SIZE_16, FFMA_OBJECT_SIZE_32, FFMA_OBJECT_SIZE_64, \
                                        FFMA_OBJECT_SIZE_128, FFMA_OBJECT_SIZE_256, FFMA_OBJECT_SIZE_512, \
                                        FFMA_OBJECT_SIZE_1024, FFMA_OBJECT_SIZE_2048, FFMA_OBJECT_SIZE_4096, \
                                        FFMA_OBJECT_SIZE_8192, FFMA_OBJECT_SIZE_16384, FFMA_OBJECT_SIZE_32768, \
                                        FFMA_OBJECT_SIZE_65536
#define FFMA_PREDEFINED_OBJECT_SIZES_COUNT (sizeof(ffma_predefined_object_sizes) / sizeof(uint32_t))

#define FFMA_OBJECT_SIZE_MIN    (((int[]){ FFMA_PREDEFINED_OBJECT_SIZES })[0])
#define FFMA_OBJECT_SIZE_MAX    (((int[]){ FFMA_PREDEFINED_OBJECT_SIZES })[FFMA_PREDEFINED_OBJECT_SIZES_COUNT - 1])

static const uint32_t ffma_predefined_object_sizes[] = { FFMA_PREDEFINED_OBJECT_SIZES };

typedef struct ffma ffma_t;
struct ffma {
    // The slots and the slices are sorted per availability
    double_linked_list_t *slots;
    double_linked_list_t *slices;
    queue_mpmc_t *free_ffma_slots_queue_from_other_threads;

    // When the thread owning an instance of an allocator is terminated, other threads might still own some memory it
    // initialized and therefore some support is needed there.
    // When a thread sends back memory to a thread it has to check if it has been terminated and if yes, process the
    // free_ffma_slots_queue, free up the  slots, check if the slice owning the
    // ffma_slot is then empty, and in case return the addres sof the page.
    // All these operations have to be carried out under the external_thread_lock spinlock to avoid contention.
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
            uint32_t objects_inuse_count;
        } metrics;
        ffma_slot_t slots[];
    } __attribute__((aligned(64))) data;
} ffma_slice_t;

#if FFMA_DEBUG_ALLOCS_FREES == 1
void ffma_debug_allocs_frees_end();
#endif

ffma_t* ffma_thread_cache_get_ffma_by_size(
        size_t object_size);

ffma_t* ffma_init(
        size_t object_size);

bool ffma_free(
        ffma_t *ffma);

uint8_t ffma_index_by_object_size(
        size_t object_size);

size_t ffma_slice_calculate_usable_page_size();

uint32_t ffma_slice_calculate_data_offset(
        size_t usable_hugepage_size,
        size_t object_size);

uint32_t ffma_slice_calculate_slots_count(
        size_t usable_hugepage_size,
        size_t data_offset,
        size_t object_size);

ffma_slice_t* ffma_slice_init(
        ffma_t *ffma,
        void *memptr);

void ffma_slice_add_slots_to_per_thread_metadata_slots(
        ffma_t *ffma,
        ffma_slice_t *ffma_slice);

void ffma_slice_remove_slots_from_per_thread_metadata_slots(
        ffma_t *ffma,
        ffma_slice_t *ffma_slice);

ffma_slice_t* ffma_slice_from_memptr(
        void *memptr);

void ffma_slice_make_available(
        ffma_t *ffma,
        ffma_slice_t *ffma_slice);

ffma_slot_t* ffma_slot_from_memptr(
        ffma_t *ffma,
        ffma_slice_t *ffma_slice,
        void *memptr);

void ffma_grow(
        ffma_t *ffma,
        void *memptr);

__attribute__((malloc))
void* ffma_mem_alloc_internal(
        ffma_t* ffma,
        size_t size);

void ffma_mem_free_page_current_thread(
        ffma_t* ffma,
        ffma_slice_t* ffma_slice,
        ffma_slot_t* ffma_slot);

void ffma_mem_free_page_different_thread(
        ffma_t* ffma,
        ffma_slot_t* ffma_slot);

__attribute__((malloc))
void* ffma_mem_alloc(
        size_t size);

void* ffma_mem_alloc_zero(
        size_t size);

void* ffma_mem_realloc(
        void *memptr,
        size_t current_size,
        size_t new_size,
        bool zero_new_memory);

void ffma_mem_free(
        void *memptr);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FFMA_H
