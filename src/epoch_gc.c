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

#include "misc.h"
#include "xalloc.h"
#include "spinlock.h"
#include "intrinsics.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/ring_bounded_spsc/ring_bounded_spsc.h"
#include "memory_allocator/ffma.h"

#include "epoch_gc.h"

thread_local epoch_gc_thread_t *thread_local_epoch_gc[EPOCH_GC_OBJECT_TYPE_MAX] = { 0 };
static epoch_gc_staged_object_destructor_cb_t* epoch_gc_staged_object_destructor_cb[] = { 0 };

epoch_gc_t *epoch_gc_init(
        epoch_gc_object_type_t object_type) {
    epoch_gc_t *epoch_gc = xalloc_alloc_zero(sizeof(epoch_gc_t));
    if (!epoch_gc) {
        return NULL;
    }

    epoch_gc->object_type = object_type;
    epoch_gc->thread_list = double_linked_list_init();
    if (!epoch_gc->thread_list) {
        xalloc_free(epoch_gc);
        return NULL;
    }

    spinlock_init(&epoch_gc->thread_list_spinlock);

    return epoch_gc;
}

void epoch_gc_free(
        epoch_gc_t *epoch_gc) {
    // TODO
}

void epoch_gc_register_object_type_destructor_cb(
        epoch_gc_object_type_t object_type,
        epoch_gc_staged_object_destructor_cb_t *destructor_cb) {
    epoch_gc_staged_object_destructor_cb[object_type] = destructor_cb;
}

bool epoch_gc_thread_append_new_staged_objects_ring(
        epoch_gc_thread_t *epoch_gc_thread) {
    ring_bounded_spsc_t *rb = ring_bounded_spsc_init(EPOCH_GC_STAGED_OBJECTS_RING_SIZE);
    if (!rb) {
        return false;
    }

    double_linked_list_item_t *rb_item = double_linked_list_item_init();
    if (!rb_item) {
        ring_bounded_spsc_free(rb);
        return false;
    }

    // Update the last ring to be used
    epoch_gc_thread->staged_objects_ring_last = rb;

    // Append the ring to the ring list
    rb_item->data = rb;
    double_linked_list_push_item(epoch_gc_thread->staged_objects_ring_list, rb_item);

    return true;
}

bool epoch_gc_register_thread(
        epoch_gc_t *epoch_gc) {
    epoch_gc_thread_t *epoch_gc_thread = NULL;
    double_linked_list_item_t *epoch_gc_thread_item = NULL;

    epoch_gc_thread = xalloc_alloc_zero(sizeof(epoch_gc_thread_t));
    if (!epoch_gc_thread) {
        goto fail;
    }

    epoch_gc_thread->epoch = 0;
    epoch_gc_thread->epoch_gc = epoch_gc;

    epoch_gc_thread->staged_objects_ring_list = double_linked_list_init();
    if (!epoch_gc_thread->staged_objects_ring_list) {
        goto fail;
    }

    epoch_gc_thread->staged_objects_ring_last = ring_bounded_spsc_init(EPOCH_GC_STAGED_OBJECTS_RING_SIZE);
    if (!epoch_gc_thread->staged_objects_ring_last) {
        goto fail;
    }

    epoch_gc_thread_item = double_linked_list_item_init();
    if (!epoch_gc_thread_item) {
        goto fail;
    }

    if (!epoch_gc_thread_append_new_staged_objects_ring(epoch_gc_thread)) {
        goto fail;
    }

    // Add the thread registration to the list of registered threads
    epoch_gc_thread_item->data = epoch_gc_thread;

    spinlock_lock(&epoch_gc->thread_list_spinlock);
    double_linked_list_push_item(epoch_gc->thread_list, epoch_gc_thread_item);
    spinlock_unlock(&epoch_gc->thread_list_spinlock);

    // Store a reference in the thread local store
    thread_local_epoch_gc[epoch_gc->object_type] = epoch_gc_thread;

    return true;

    fail:
    if (epoch_gc_thread && epoch_gc_thread->staged_objects_ring_list) {
        double_linked_list_free(epoch_gc_thread->staged_objects_ring_list);
    }

    if (epoch_gc_thread && epoch_gc_thread->staged_objects_ring_last) {
        ring_bounded_spsc_free(epoch_gc_thread->staged_objects_ring_last);
    }

    if (epoch_gc_thread) {
        xalloc_free(epoch_gc_thread);
    }

    if (epoch_gc_thread_item) {
        double_linked_list_item_free(epoch_gc_thread_item);
    }

    return false;
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
        epoch_gc_object_type_t object_type) {
    epoch_gc_t *epoch_gc = NULL;
    epoch_gc_thread_t *epoch_gc_thread = NULL;

    epoch_gc_thread_get_instance(object_type, &epoch_gc, &epoch_gc_thread);

    MEMORY_FENCE_STORE();

    return epoch_gc_thread->thread_terminated;
}

void epoch_gc_thread_terminate(
        epoch_gc_object_type_t object_type) {
    epoch_gc_t *epoch_gc = NULL;
    epoch_gc_thread_t *epoch_gc_thread = NULL;

    // Tries to collect all before
    epoch_gc_thread_collect_all(object_type);

    epoch_gc_thread_get_instance(object_type, &epoch_gc, &epoch_gc_thread);

    epoch_gc_thread->thread_terminated = true;
    MEMORY_FENCE_STORE();
}

void epoch_gc_thread_advance_epoch(
        epoch_gc_object_type_t object_type) {
    epoch_gc_t *epoch_gc = NULL;
    epoch_gc_thread_t *epoch_gc_thread = NULL;

    epoch_gc_thread_get_instance(object_type, &epoch_gc, &epoch_gc_thread);

    epoch_gc_thread->epoch = intrinsics_tsc();
    MEMORY_FENCE_STORE();
}

void epoch_gc_thread_destruct_staged_objects_batch(
        epoch_gc_staged_object_destructor_cb_t *destructor_cb,
        uint8_t staged_objects_counter,
        epoch_gc_staged_object_t *staged_objects[EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE]) {
    destructor_cb(staged_objects_counter, staged_objects);

    for(
            uint8_t staged_object_index = 0;
            staged_object_index < staged_objects_counter;
            staged_object_index++) {
        ffma_mem_free(staged_objects[staged_object_index]);
    }
}

uint32_t epoch_gc_thread_collect(
        epoch_gc_object_type_t object_type,
        uint32_t max_items) {
    uint32_t deleted_counter = 0;
    epoch_gc_staged_object_t *staged_objects[EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE];
    uint8_t staged_objects_counter = 0;
    epoch_gc_t *epoch_gc = NULL;
    epoch_gc_thread_t *epoch_gc_thread = NULL;

    epoch_gc_thread_get_instance(object_type, &epoch_gc, &epoch_gc_thread);

    // Get the lowest epoch among all the threads registered for this object type
    uint64_t epoch = UINT64_MAX;
    double_linked_list_item_t* epoch_gc_thread_item = NULL;

    // Critical section operating on the thread list
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

    // Can't iterate over the double linked list normally as items can be deleted as part of the process
    double_linked_list_t *list = epoch_gc_thread->staged_objects_ring_list;
    double_linked_list_item_t* item = list->head;
    while(item != NULL) {
        epoch_gc_staged_object_t *staged_object;
        bool stop = false;
        ring_bounded_spsc_t *staged_objects_ring = item->data;

        // Peek, instead of dequeue, to avoid fetching an item that can't be destroyed as potentially it's in use
        while(likely((staged_object = ring_bounded_spsc_peek(staged_objects_ring)) != NULL)) {
            if (epoch <= staged_object->epoch || deleted_counter > max_items) {
                stop = true;
                break;
            }

            // Remove the fetched object from the queue
            ring_bounded_spsc_dequeue(staged_objects_ring);

            staged_objects[staged_objects_counter] = staged_object;
            staged_objects_counter++;
            if (staged_objects_counter == ARRAY_SIZE(staged_objects)) {
                epoch_gc_thread_destruct_staged_objects_batch(
                        epoch_gc_staged_object_destructor_cb[object_type],
                        staged_objects_counter,
                        staged_objects);

                deleted_counter += staged_objects_counter;
                staged_objects_counter = 0;
            }

            deleted_counter++;
        }

        if (stop) {
            break;
        }

        // Before checking if the ring can be removed store the current item in a new variable and get the next one
        double_linked_list_item_t *old_item = item;
        item = item->next;

        // TODO: the check on the ring useless as the only way to get to this point is if the ring is empty but needs
        //       testing, so better to leave this check around. The part of the if that checks if there is only one
        //       ring is required as if it's the last one we don't really want to remove it.
        //       It's not necessary to update staged_objects_ring_last as that's always point to the last one and, per
        //       definition, when processing the double linked list of rings if the code get to the last one the check
        //       will avoid removing it.
        if (list->count > 1 && ring_bounded_spsc_is_empty(staged_objects_ring)) {
            double_linked_list_remove_item(list, old_item);
            double_linked_list_item_free(old_item);
            ring_bounded_spsc_free(staged_objects_ring);
        }
    }

    if (staged_objects_counter > 0) {
        epoch_gc_thread_destruct_staged_objects_batch(
                epoch_gc_staged_object_destructor_cb[object_type],
                staged_objects_counter,
                staged_objects);

        deleted_counter += staged_objects_counter;
    }

    return deleted_counter;
}

uint32_t epoch_gc_thread_collect_all(
        epoch_gc_object_type_t object_type) {
    return epoch_gc_thread_collect(object_type, UINT32_MAX);
}

bool epoch_gc_stage_object(
        epoch_gc_object_type_t object_type,
        void* object) {
    epoch_gc_t *epoch_gc = NULL;
    epoch_gc_thread_t *epoch_gc_thread = NULL;

    epoch_gc_thread_get_instance(object_type, &epoch_gc, &epoch_gc_thread);

    // Allocate the staged pointer
    epoch_gc_staged_object_t *staged_object = ffma_mem_alloc(sizeof(epoch_gc_staged_object_t));
    staged_object->epoch = intrinsics_tsc();
    staged_object->object = object;

    // Try to insert the pointer into the last available ring
    if (unlikely(!ring_bounded_spsc_enqueue(epoch_gc_thread->staged_objects_ring_last, staged_object))) {
        // If the operation fails a new ring has to be appended and the operation retried
        if (unlikely(!epoch_gc_thread_append_new_staged_objects_ring(epoch_gc_thread))) {
            return false;
        }

        if (unlikely(!ring_bounded_spsc_enqueue(epoch_gc_thread->staged_objects_ring_last, staged_object))) {
            // Can't really happen as the ring is brand new and therefore empty, but better to always check the return
            // values. This is also the slow path so an additional branch is not a particular problem.
            return false;
        }
    }

    return true;
}
