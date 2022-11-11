#ifndef CACHEGRAND_EPOCH_GC_H
#define CACHEGRAND_EPOCH_GC_H

#ifdef __cplusplus
extern "C" {
#endif

#define EPOCH_GC_STAGED_OBJECTS_RING_SIZE (16 * 1024)

// Tests depend on having a batch size of at least 4
#define EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE (16)

enum epoch_gc_object_type {
    EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE,
    EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX,
    EPOCH_GC_OBJECT_TYPE_MAX,
};
typedef enum epoch_gc_object_type epoch_gc_object_type_t;

typedef struct epoch_gc epoch_gc_t;
struct epoch_gc {
    double_linked_list_t *thread_list;
    uint64_t thread_list_change_epoch;
    epoch_gc_object_type_t object_type;
    spinlock_lock_volatile_t thread_list_spinlock;
};

typedef struct epoch_gc_thread epoch_gc_thread_t;
struct epoch_gc_thread {
    ring_bounded_spsc_t *staged_objects_ring_last;
    double_linked_list_t *staged_objects_ring_list;
    uint64_t epoch;
    epoch_gc_t *epoch_gc;
    spinlock_lock_volatile_t staged_objects_ring_list_spinlock;
    bool thread_terminated;
};

typedef struct epoch_gc_staged_object epoch_gc_staged_object_t;
struct epoch_gc_staged_object {
    uint64_t epoch;
    void *object;
};

typedef void (epoch_gc_staged_object_destructor_cb_t)(
        uint8_t,
        epoch_gc_staged_object_t*[EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE]);

#if DEBUG == 1
// Used only for testing and debugging
epoch_gc_thread_t** epoch_gc_get_thread_local_epoch_gc();

epoch_gc_staged_object_destructor_cb_t** epoch_gc_get_epoch_gc_staged_object_destructor_cb();
#endif

epoch_gc_t *epoch_gc_init(
        epoch_gc_object_type_t object_type);

void epoch_gc_free(
        epoch_gc_t *epoch_gc);

void epoch_gc_register_object_type_destructor_cb(
        epoch_gc_object_type_t object_type,
        epoch_gc_staged_object_destructor_cb_t *destructor_cb);

void epoch_gc_unregister_object_type_destructor_cb(
        epoch_gc_object_type_t object_type);

void epoch_gc_thread_append_new_staged_objects_ring(
        epoch_gc_thread_t *epoch_gc_thread);

epoch_gc_thread_t *epoch_gc_thread_init();

void epoch_gc_thread_free(
        epoch_gc_thread_t *epoch_gc_thread);

void epoch_gc_thread_register_global(
        epoch_gc_t *epoch_gc,
        epoch_gc_thread_t *epoch_gc_thread);

void epoch_gc_thread_unregister_global(
        epoch_gc_thread_t *epoch_gc_thread);

void epoch_gc_thread_register_local(
        epoch_gc_thread_t *epoch_gc_thread);

void epoch_gc_thread_unregister_local(
        epoch_gc_thread_t *epoch_gc_thread);

void epoch_gc_thread_get_instance(
        epoch_gc_object_type_t object_type,
        epoch_gc_t **epoch_gc,
        epoch_gc_thread_t **epoch_gc_thread);

bool epoch_gc_thread_is_terminated(
        epoch_gc_thread_t *epoch_gc_thread);

void epoch_gc_thread_terminate(
        epoch_gc_thread_t *epoch_gc_thread);

void epoch_gc_thread_advance_epoch_tsc(
        epoch_gc_thread_t *epoch_gc_thread);

void epoch_gc_thread_advance_epoch_by_one(
        epoch_gc_thread_t *epoch_gc_thread);

void epoch_gc_thread_destruct_staged_objects_batch(
        epoch_gc_staged_object_destructor_cb_t *destructor_cb,
        uint8_t staged_objects_counter,
        epoch_gc_staged_object_t *staged_objects[EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE]);

uint32_t epoch_gc_thread_collect(
        epoch_gc_thread_t *epoch_gc_thread,
        uint32_t max_objects);

uint32_t epoch_gc_thread_collect_all(
        epoch_gc_thread_t *epoch_gc_thread);

bool epoch_gc_stage_object(
        epoch_gc_object_type_t object_type,
        void* object);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_EPOCH_GC_H
