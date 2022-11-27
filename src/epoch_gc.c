/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>

#include "misc.h"
#include "xalloc.h"
#include "spinlock.h"
#include "intrinsics.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"

#include "epoch_gc.h"

typedef struct ring_list_item_map ring_list_item_map_t;
struct ring_list_item_map {
    ring_bounded_queue_spsc_uint128_t *ring;
    double_linked_list_item_t *item;
    bool to_delete;
};

thread_local epoch_gc_thread_t *thread_local_epoch_gc[EPOCH_GC_OBJECT_TYPE_MAX] = {
        NULL
};
epoch_gc_staged_object_destructor_cb_t* epoch_gc_staged_object_destructor_cb[EPOCH_GC_OBJECT_TYPE_MAX] = {
        NULL
};

#if DEBUG == 1
epoch_gc_thread_t** epoch_gc_get_thread_local_epoch_gc() {
    return thread_local_epoch_gc;
}

epoch_gc_staged_object_destructor_cb_t** epoch_gc_get_epoch_gc_staged_object_destructor_cb() {
    return epoch_gc_staged_object_destructor_cb;
}
#endif

epoch_gc_t *epoch_gc_init(
        epoch_gc_object_type_t object_type) {
    epoch_gc_t *epoch_gc = xalloc_alloc_zero( sizeof(epoch_gc_t));
    epoch_gc->object_type = object_type;
    epoch_gc->thread_list = double_linked_list_init();
    epoch_gc->thread_list_change_epoch = 0;

    spinlock_init(&epoch_gc->thread_list_spinlock);

    return epoch_gc;
}

void epoch_gc_free(
        epoch_gc_t *epoch_gc) {
    double_linked_list_free(epoch_gc->thread_list);
    xalloc_free(epoch_gc);
}

void epoch_gc_register_object_type_destructor_cb(
        epoch_gc_object_type_t object_type,
        epoch_gc_staged_object_destructor_cb_t *destructor_cb) {
    epoch_gc_staged_object_destructor_cb[object_type] = destructor_cb;
}

void epoch_gc_unregister_object_type_destructor_cb(
        epoch_gc_object_type_t object_type) {
    epoch_gc_staged_object_destructor_cb[object_type] = NULL;
}

void epoch_gc_thread_append_new_staged_objects_ring(
        epoch_gc_thread_t *epoch_gc_thread) {
    ring_bounded_queue_spsc_uint128_t *rb = ring_bounded_queue_spsc_uint128_init(EPOCH_GC_STAGED_OBJECTS_RING_SIZE);

    // Initialize the new double linked list item
    double_linked_list_item_t *rb_item = double_linked_list_item_init();
    rb_item->data = rb;

    // Critical section to update the ring list and the ring to be used
    spinlock_lock(&epoch_gc_thread->staged_objects_ring_list_spinlock);
    epoch_gc_thread->staged_objects_ring_last = rb;
    double_linked_list_push_item(epoch_gc_thread->staged_objects_ring_list, rb_item);
    spinlock_unlock(&epoch_gc_thread->staged_objects_ring_list_spinlock);
}

epoch_gc_thread_t *epoch_gc_thread_init() {
    epoch_gc_thread_t *epoch_gc_thread = NULL;

    // Initialize the epoch gc thread structure
    epoch_gc_thread = xalloc_alloc_zero( sizeof(epoch_gc_thread_t));
    epoch_gc_thread->epoch = 0;
    epoch_gc_thread->staged_objects_ring_list = double_linked_list_init();
    epoch_gc_thread->staged_objects_ring_last = ring_bounded_queue_spsc_uint128_init(EPOCH_GC_STAGED_OBJECTS_RING_SIZE);
    spinlock_init(&epoch_gc_thread->staged_objects_ring_list_spinlock);

    // Initialize the ring a new ring for the staged objects
    epoch_gc_thread_append_new_staged_objects_ring(epoch_gc_thread);

    return epoch_gc_thread;
}

void epoch_gc_thread_free(
        epoch_gc_thread_t *epoch_gc_thread) {
    double_linked_list_item_t *item = epoch_gc_thread->staged_objects_ring_list->head;
    while(item != NULL) {
        double_linked_list_item_t *current = item;
        ring_bounded_queue_spsc_uint128_t *ring = (ring_bounded_queue_spsc_uint128_t*)current->data;
        item = item->next;

        // When freeing the epoch_gc_thread structure there should NEVER be staged objects in the ring
        assert(ring_bounded_queue_spsc_uint128_get_length(ring) == 0);

        ring_bounded_queue_spsc_uint128_free(ring);
        double_linked_list_item_free(current);
    }

    double_linked_list_free(epoch_gc_thread->staged_objects_ring_list);

    // No need to free staged_objects_ring_last as it's included in the list that frees the rings above
    xalloc_free(epoch_gc_thread);
}

void epoch_gc_thread_register_global(
        epoch_gc_t *epoch_gc,
        epoch_gc_thread_t *epoch_gc_thread) {
    assert(epoch_gc != NULL);
    assert(epoch_gc_thread != NULL);
    double_linked_list_item_t *epoch_gc_thread_item = double_linked_list_item_init();
    epoch_gc_thread_item->data = epoch_gc_thread;

    epoch_gc_thread->epoch_gc = epoch_gc;

    // Add the thread to the thread list of the gc
    spinlock_lock(&epoch_gc->thread_list_spinlock);
    double_linked_list_push_item(epoch_gc->thread_list, epoch_gc_thread_item);
    epoch_gc->thread_list_change_epoch = intrinsics_tsc();
    spinlock_unlock(&epoch_gc->thread_list_spinlock);
}

void epoch_gc_thread_register_local(
        epoch_gc_thread_t *epoch_gc_thread) {
    assert(epoch_gc_thread != NULL);
    epoch_gc_t *epoch_gc = epoch_gc_thread->epoch_gc;
    epoch_gc_object_type_t object_type = epoch_gc->object_type;
    assert(thread_local_epoch_gc[object_type] == NULL);
    thread_local_epoch_gc[object_type] = epoch_gc_thread;
}

void epoch_gc_thread_unregister_global(
        epoch_gc_thread_t *epoch_gc_thread) {
    epoch_gc_t *epoch_gc = epoch_gc_thread->epoch_gc;

    // Lock the thread list
    spinlock_lock(&epoch_gc->thread_list_spinlock);

    // Loop over the list to search and remove the item referencing epoch gc thread, after the removal the iterator
    // can't be used anymore as the current item is destroyed and the loop HAS to break.
    double_linked_list_item_t *item = NULL;
    while((item = double_linked_list_iter_next(epoch_gc->thread_list, item)) != NULL) {
        if (item->data != epoch_gc_thread) {
            continue;
        }

        double_linked_list_remove_item(epoch_gc->thread_list, item);
        double_linked_list_item_free(item);
        break;
    }

    epoch_gc->thread_list_change_epoch = intrinsics_tsc();

    // Unlock the thread list
    spinlock_unlock(&epoch_gc->thread_list_spinlock);

    epoch_gc_thread->epoch_gc = NULL;
}

void epoch_gc_thread_unregister_local(
        epoch_gc_thread_t *epoch_gc_thread) {
    epoch_gc_t *epoch_gc = epoch_gc_thread->epoch_gc;
    epoch_gc_object_type_t object_type = epoch_gc->object_type;
    assert(thread_local_epoch_gc[object_type] != NULL);
    thread_local_epoch_gc[object_type] = NULL;
}

void epoch_gc_thread_get_instance(
        epoch_gc_object_type_t object_type,
        epoch_gc_t **epoch_gc,
        epoch_gc_thread_t **epoch_gc_thread) {
    *epoch_gc_thread = thread_local_epoch_gc[object_type];
    assert(*epoch_gc_thread);

    *epoch_gc = (*epoch_gc_thread)->epoch_gc;
    assert((*epoch_gc)->object_type == object_type);
}

bool epoch_gc_thread_is_terminated(
        epoch_gc_thread_t *epoch_gc_thread) {
    MEMORY_FENCE_LOAD();
    return epoch_gc_thread->thread_terminated;
}

void epoch_gc_thread_terminate(
        epoch_gc_thread_t *epoch_gc_thread) {
    epoch_gc_thread->thread_terminated = true;
    MEMORY_FENCE_STORE();
}

void epoch_gc_thread_set_epoch(
        epoch_gc_thread_t *epoch_gc_thread,
        uint64_t epoch) {
    epoch_gc_thread->epoch = epoch;
    MEMORY_FENCE_STORE();
}

void epoch_gc_thread_advance_epoch_tsc(
        epoch_gc_thread_t *epoch_gc_thread) {
    epoch_gc_thread_set_epoch(epoch_gc_thread, intrinsics_tsc());
}

void epoch_gc_thread_advance_epoch_by_one(
        epoch_gc_thread_t *epoch_gc_thread) {
    epoch_gc_thread_set_epoch(epoch_gc_thread, ++epoch_gc_thread->epoch);
}

uint32_t epoch_gc_thread_collect(
        epoch_gc_thread_t *epoch_gc_thread,
        uint32_t max_objects) {
    epoch_gc_staged_object_t staged_object;
    uint32_t deleted_counter = 0;
    epoch_gc_staged_object_t staged_objects_to_delete[EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE];
    uint8_t staged_objects_to_delete_counter = 0;
    epoch_gc_t *epoch_gc = epoch_gc_thread->epoch_gc;

    // Get the lowest epoch among all the threads registered for this object type
    uint64_t epoch = UINT64_MAX;
    double_linked_list_item_t* epoch_gc_thread_item = NULL;

    // Critical section operating on the thread list to collect the lowest epoch
    // TODO: move this outside and change collect and collect all to get the starting epoch as parameter

    spinlock_lock(&epoch_gc->thread_list_spinlock);
    while((epoch_gc_thread_item = double_linked_list_iter_next(
            epoch_gc->thread_list, epoch_gc_thread_item)) != NULL) {
        MEMORY_FENCE_LOAD();
        uint64_t thread_epoch = epoch_gc_thread->epoch;

        if (thread_epoch < epoch) {
            epoch = thread_epoch;
        }
    }
    spinlock_unlock(&epoch_gc->thread_list_spinlock);

    // End of the critical section

    // if there is only one ring, the code path can be much simpler and faster and will normally be the case
    MEMORY_FENCE_LOAD();
    if (likely(epoch_gc_thread->staged_objects_ring_list->count == 1)) {
        // The ring is loaded from staged_objects_ring_list->head instead that from staged_objects_ring_last just in
        // case a new ring get added right after reading the count.
        ring_bounded_queue_spsc_uint128_t *staged_objects_ring = epoch_gc_thread->staged_objects_ring_list->head->data;

        // Peek, instead of dequeue, to avoid fetching an item that can't be destroyed as potentially it's in use
        while(true) {
            bool found = false;
            staged_object._packed = ring_bounded_queue_spsc_uint128_peek(staged_objects_ring, &found);

            if (unlikely(!found)) {
                break;
            }

            if (epoch <= staged_object.data.epoch || deleted_counter >= max_objects) {
                break;
            }

            // Remove the fetched object from the queue
            ring_bounded_queue_spsc_uint128_dequeue(staged_objects_ring, NULL);

            staged_objects_to_delete[staged_objects_to_delete_counter]._packed = staged_object._packed;
            staged_objects_to_delete_counter++;
            deleted_counter++;
            if (staged_objects_to_delete_counter == ARRAY_SIZE(staged_objects_to_delete)) {
                epoch_gc_staged_object_destructor_cb[epoch_gc->object_type](
                        staged_objects_to_delete_counter, staged_objects_to_delete);
                staged_objects_to_delete_counter = 0;
            }
        }
    } else {
        // Critical section to acquire the list of rings
        spinlock_lock(&epoch_gc_thread->staged_objects_ring_list_spinlock);

        // Allocate a map of ring_list_item_map_t to fetch the items and the amount of times a lock is needed
        uint32_t ring_list_item_map_index = 0;
        uint32_t ring_list_item_map_length = epoch_gc_thread->staged_objects_ring_list->count;
        ring_list_item_map_t *ring_list_item_map_list = xalloc_alloc_zero(
                ring_list_item_map_length * sizeof(ring_list_item_map_t));

        double_linked_list_item_t* item = NULL;
        while((item = double_linked_list_iter_next(
                epoch_gc_thread->staged_objects_ring_list, item)) != NULL) {
            ring_list_item_map_list[ring_list_item_map_index].ring = item->data;
            ring_list_item_map_list[ring_list_item_map_index].item = item;
            ring_list_item_map_list[ring_list_item_map_index].to_delete = false;
            ring_list_item_map_index++;
        }

        // End critical section to acquire the list of rings
        spinlock_unlock(&epoch_gc_thread->staged_objects_ring_list_spinlock);

        for(
                ring_list_item_map_index = 0;
                ring_list_item_map_index < ring_list_item_map_length;
                ring_list_item_map_index++) {
            bool stop = false;
            ring_bounded_queue_spsc_uint128_t *staged_objects_ring = ring_list_item_map_list[ring_list_item_map_index].ring;

            // Peek, instead of dequeue, to avoid fetching an item that can't be destroyed as potentially it's in use
            while(true) {
                bool found = false;
                staged_object._packed = ring_bounded_queue_spsc_uint128_peek(staged_objects_ring, &found);

                if (unlikely(!found)) {
                    break;
                }

                if (epoch <= staged_object.data.epoch || deleted_counter >= max_objects) {
                    stop = true;
                    break;
                }

                // Remove the fetched object from the queue
                ring_bounded_queue_spsc_uint128_dequeue(staged_objects_ring, NULL);

                staged_objects_to_delete[staged_objects_to_delete_counter] = staged_object;
                staged_objects_to_delete_counter++;
                if (staged_objects_to_delete_counter == ARRAY_SIZE(staged_objects_to_delete)) {
                    epoch_gc_staged_object_destructor_cb[epoch_gc->object_type](
                            staged_objects_to_delete_counter, staged_objects_to_delete);
                    staged_objects_to_delete_counter = 0;
                }

                deleted_counter++;
            }

            if (stop) {
                break;
            }

            ring_list_item_map_list[ring_list_item_map_index].to_delete = true;
        }

        // Critical section to delete the empty rings
        spinlock_lock(&epoch_gc_thread->staged_objects_ring_list_spinlock);

        for(
                ring_list_item_map_index = 0;
                ring_list_item_map_index < ring_list_item_map_length;
                ring_list_item_map_index++) {
            ring_bounded_queue_spsc_uint128_t *staged_objects_ring = ring_list_item_map_list[ring_list_item_map_index].ring;

            // Don't delete the active ring
            if (ring_list_item_map_list[ring_list_item_map_index].to_delete == false ||
                    staged_objects_ring == epoch_gc_thread->staged_objects_ring_last) {
                continue;
            }

            double_linked_list_remove_item(
                    epoch_gc_thread->staged_objects_ring_list,
                    ring_list_item_map_list[ring_list_item_map_index].item);
            double_linked_list_item_free(ring_list_item_map_list[ring_list_item_map_index].item);
            ring_bounded_queue_spsc_uint128_free(ring_list_item_map_list[ring_list_item_map_index].ring);
        }

        spinlock_unlock(&epoch_gc_thread->staged_objects_ring_list_spinlock);

        // End of the critical section

        xalloc_free(ring_list_item_map_list);
    }

    if (staged_objects_to_delete_counter > 0) {
        epoch_gc_staged_object_destructor_cb[epoch_gc->object_type](
                staged_objects_to_delete_counter, staged_objects_to_delete);
    }

    return deleted_counter;
}

uint32_t epoch_gc_thread_collect_all(
        epoch_gc_thread_t *epoch_gc_thread) {
    return epoch_gc_thread_collect(epoch_gc_thread, UINT32_MAX);
}

bool epoch_gc_stage_object(
        epoch_gc_object_type_t object_type,
        void* object) {
    epoch_gc_t *epoch_gc = NULL;
    epoch_gc_thread_t *epoch_gc_thread = NULL;

    epoch_gc_thread_get_instance(object_type, &epoch_gc, &epoch_gc_thread);

    // Allocate the staged pointer
    epoch_gc_staged_object_t staged_object = {
            .data = {
                    .epoch = epoch_gc_thread->epoch,
                    .object = object,
            }
    };

    // Try to insert the pointer into the last available ring
    if (unlikely(!ring_bounded_queue_spsc_uint128_enqueue(epoch_gc_thread->staged_objects_ring_last, staged_object._packed))) {
        // If the operation fails a new ring has to be appended and the operation retried
        epoch_gc_thread_append_new_staged_objects_ring(epoch_gc_thread);

        if (unlikely(!ring_bounded_queue_spsc_uint128_enqueue(epoch_gc_thread->staged_objects_ring_last, staged_object._packed))) {
            // Can't really happen as the ring is brand new and therefore empty, but better to always check the return
            // values. This is also the slow path so an additional branch is not a particular problem.
            return false;
        }
    }

    return true;
}
