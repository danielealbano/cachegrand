/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include <cstdint>
#include <cstdbool>
#include <csignal>
#include <csetjmp>

#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/ring_bounded_spsc/ring_bounded_spsc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "xalloc.h"
#include "signals_support.h"

#include "epoch_gc.h"

sigjmp_buf test_epoch_gc_jump_fp;
void test_epoch_gc_signal_sigabrt_and_sigsegv_handler_longjmp(int) {
    siglongjmp(test_epoch_gc_jump_fp, 1);
}

void test_epoch_gc_signal_sigabrt_and_sigsegv_handler_setup() {
    signals_support_register_signal_handler(
            SIGABRT,
            test_epoch_gc_signal_sigabrt_and_sigsegv_handler_longjmp,
            nullptr);

    signals_support_register_signal_handler(
            SIGSEGV,
            test_epoch_gc_signal_sigabrt_and_sigsegv_handler_longjmp,
            nullptr);
}

static uint8_t test_epoch_gc_object_destructor_cb_params_staged_objects_count = 0;
static epoch_gc_staged_object_t **test_epoch_gc_object_destructor_cb_params_staged_objects;
void test_epoch_gc_object_destructor_cb(
        uint8_t staged_objects_count,
        epoch_gc_staged_object_t *staged_objects[EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE]) {
    test_epoch_gc_object_destructor_cb_params_staged_objects_count = staged_objects_count;
    test_epoch_gc_object_destructor_cb_params_staged_objects = staged_objects;

    for(uint64_t i = 0; i < EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE; i++) {
        REQUIRE(staged_objects[i]->object == (void*)i);
    }
}

TEST_CASE("epoch_gc.c", "[epoch-gc]") {
    SECTION("epoch_gc_init") {
        SECTION("valid object type") {
            epoch_gc_t *epoch_gc = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX);

            REQUIRE(epoch_gc->object_type == EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX);
            REQUIRE(spinlock_is_locked(&epoch_gc->thread_list_spinlock) == false);
            REQUIRE(epoch_gc->thread_list != NULL);

            double_linked_list_free(epoch_gc->thread_list);
            xalloc_free(epoch_gc);
        }

#if DEBUG == 1
        SECTION("invalid object type") {
            bool fatal_caught = false;
            if (sigsetjmp(test_epoch_gc_jump_fp, 1) == 0) {
                test_epoch_gc_signal_sigabrt_and_sigsegv_handler_setup();
                epoch_gc_init((epoch_gc_object_type_t)UINT32_MAX);
            } else {
                fatal_caught = true;
            }

            // The fatal_caught variable has to be set to true as sigsetjmp on the second execution will return a value
            // different from zero.
            // A sigsegv raised by the kernel because of the memory protection means that the stack overflow protection
            // is working as intended
            REQUIRE(fatal_caught);
        }
#endif
    }

    SECTION("epoch_gc_register_object_type_destructor_cb") {
        SECTION("valid object type and valid function pointer") {
            epoch_gc_register_object_type_destructor_cb(
                    EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX,
                    test_epoch_gc_object_destructor_cb);

            REQUIRE(epoch_gc_staged_object_destructor_cb[EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX] ==
                test_epoch_gc_object_destructor_cb);

            epoch_gc_staged_object_destructor_cb[EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX] = nullptr;
        }

#if DEBUG == 1
        SECTION("invalid object type") {
            bool fatal_caught = false;
            if (sigsetjmp(test_epoch_gc_jump_fp, 1) == 0) {
                test_epoch_gc_signal_sigabrt_and_sigsegv_handler_setup();
                epoch_gc_register_object_type_destructor_cb(
                        (epoch_gc_object_type_t)UINT32_MAX,
                        test_epoch_gc_object_destructor_cb);
            } else {
                fatal_caught = true;
            }

            // The fatal_caught variable has to be set to true as sigsetjmp on the second execution will return a value
            // different from zero.
            // A sigsegv raised by the kernel because of the memory protection means that the stack overflow protection
            // is working as intended
            REQUIRE(fatal_caught);
        }
#endif
    }

    SECTION("epoch_gc_thread_append_new_staged_objects_ring") {
        epoch_gc_thread_t epoch_gc_thread = { nullptr };
        epoch_gc_thread.staged_objects_ring_list = double_linked_list_init();

        SECTION("append 1 new ring") {
            epoch_gc_thread_append_new_staged_objects_ring(&epoch_gc_thread);

            REQUIRE(epoch_gc_thread.staged_objects_ring_list->count == 1);
            REQUIRE(epoch_gc_thread.staged_objects_ring_list->tail->data == epoch_gc_thread.staged_objects_ring_last);
            REQUIRE(ring_bounded_spsc_get_length(epoch_gc_thread.staged_objects_ring_last) == 0);
        }

        SECTION("append 2 new rings") {
            epoch_gc_thread_append_new_staged_objects_ring(&epoch_gc_thread);

            REQUIRE(epoch_gc_thread.staged_objects_ring_list->count == 2);
            REQUIRE(epoch_gc_thread.staged_objects_ring_list->tail->data == epoch_gc_thread.staged_objects_ring_last);
            REQUIRE(ring_bounded_spsc_get_length(epoch_gc_thread.staged_objects_ring_last) == 0);
        }

        SECTION("append multiple new rings") {
            for(int i = 0; i < 64; i++) {
                epoch_gc_thread_append_new_staged_objects_ring(&epoch_gc_thread);
            }

            REQUIRE(epoch_gc_thread.staged_objects_ring_list->count == 64);
            REQUIRE(epoch_gc_thread.staged_objects_ring_list->tail->data == epoch_gc_thread.staged_objects_ring_last);
            REQUIRE(ring_bounded_spsc_get_length(epoch_gc_thread.staged_objects_ring_last) == 0);
        }

        double_linked_list_item_t *rb_item = epoch_gc_thread.staged_objects_ring_list->head;
        while(rb_item != nullptr) {
            double_linked_list_item_t *current = rb_item;
            rb_item = rb_item->next;

            ring_bounded_spsc_free((ring_bounded_spsc_t*)current->data);
            double_linked_list_item_free(current);
        }

        double_linked_list_free(epoch_gc_thread.staged_objects_ring_list);
    }

    SECTION("epoch_gc_thread_init") {
        epoch_gc_t epoch_gc = { 0 };
        epoch_gc.thread_list = double_linked_list_init();
        epoch_gc.object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX;
        spinlock_init(&epoch_gc.thread_list_spinlock);

        SECTION("create one thread") {
            auto epoch_gc_thread = epoch_gc_thread_init(&epoch_gc);

            REQUIRE(epoch_gc_thread != nullptr);
            REQUIRE(epoch_gc_thread->epoch == 0);
            REQUIRE(epoch_gc_thread->staged_objects_ring_list->count == 1);
            REQUIRE(epoch_gc_thread->staged_objects_ring_list->tail->data == epoch_gc_thread->staged_objects_ring_last);
            REQUIRE(ring_bounded_spsc_get_length(epoch_gc_thread->staged_objects_ring_last) == 0);
        }

        // Free the thread list
        double_linked_list_free(epoch_gc.thread_list);
    }

    SECTION("epoch_gc_thread_register_global") {
        epoch_gc_t epoch_gc = { 0 };
        epoch_gc.thread_list = double_linked_list_init();
        epoch_gc.object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX;
        spinlock_init(&epoch_gc.thread_list_spinlock);

        epoch_gc_thread_t *epoch_gc_thread_1 = epoch_gc_thread_init(&epoch_gc);
        epoch_gc_thread_t *epoch_gc_thread_2 = epoch_gc_thread_init(&epoch_gc);
        epoch_gc_thread_t *epoch_gc_thread_3 = epoch_gc_thread_init(&epoch_gc);

        SECTION("register one thread") {
            epoch_gc_thread_register_global(&epoch_gc, epoch_gc_thread_1);

            REQUIRE(epoch_gc_thread_1->epoch_gc == &epoch_gc);
            REQUIRE(epoch_gc.thread_list->count == 1);
            REQUIRE(epoch_gc.thread_list->tail->data == epoch_gc_thread_1);
        }

        SECTION("register two thread") {
            epoch_gc_thread_register_global(&epoch_gc, epoch_gc_thread_1);
            epoch_gc_thread_register_global(&epoch_gc, epoch_gc_thread_2);

            REQUIRE(epoch_gc_thread_1->epoch_gc == &epoch_gc);
            REQUIRE(epoch_gc_thread_2->epoch_gc == &epoch_gc);
            REQUIRE(epoch_gc.thread_list->count == 2);
            REQUIRE(epoch_gc.thread_list->tail->data == epoch_gc_thread_2);
        }

        SECTION("register multiple threads") {
            epoch_gc_thread_register_global(&epoch_gc, epoch_gc_thread_1);
            epoch_gc_thread_register_global(&epoch_gc, epoch_gc_thread_2);
            epoch_gc_thread_register_global(&epoch_gc, epoch_gc_thread_3);

            REQUIRE(epoch_gc_thread_1->epoch_gc == &epoch_gc);
            REQUIRE(epoch_gc_thread_2->epoch_gc == &epoch_gc);
            REQUIRE(epoch_gc_thread_3->epoch_gc == &epoch_gc);
            REQUIRE(epoch_gc.thread_list->count == 3);
            REQUIRE(epoch_gc.thread_list->tail->data == epoch_gc_thread_3);
        }

        // Free up the list of threads and the related data
        double_linked_list_item_t *item = epoch_gc.thread_list->head;
        while(item != nullptr) {
            double_linked_list_item_t *current = item;
            item = item->next;

            epoch_gc_thread_free((epoch_gc_thread_t*)current->data);
            double_linked_list_item_free(current);
        }

        // Free the thread list
        double_linked_list_free(epoch_gc.thread_list);
    }

    SECTION("epoch_gc_thread_register_local") {
        epoch_gc_t epoch_gc = {
                .object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX,
        };

        epoch_gc_thread_t epoch_gc_thread = {
                .epoch_gc = &epoch_gc,
        };

        epoch_gc_thread_register_local(&epoch_gc_thread);

        REQUIRE(thread_local_epoch_gc[epoch_gc.object_type] == &epoch_gc_thread);

        thread_local_epoch_gc[epoch_gc.object_type] = nullptr;
    }

    SECTION("epoch_gc_thread_get_instance") {
        epoch_gc_t epoch_gc = {
                .object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX,
        };

        epoch_gc_thread_t epoch_gc_thread = { nullptr };

        epoch_gc_thread_register_local(&epoch_gc_thread);

        epoch_gc_t *epoch_gc_new = nullptr;
        epoch_gc_thread_t *epoch_gc_thread_new = nullptr;

        epoch_gc_thread_get_instance(
                EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX,
                &epoch_gc_new,
                &epoch_gc_thread_new);

        REQUIRE(&epoch_gc == epoch_gc_new);
        REQUIRE(&epoch_gc_thread == epoch_gc_thread_new);

        thread_local_epoch_gc[epoch_gc.object_type] = nullptr;
    }

    SECTION("epoch_gc_thread_is_terminated") {
        epoch_gc_t epoch_gc = {
                .object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX,
        };

        epoch_gc_thread_t epoch_gc_thread = {
                .epoch_gc = &epoch_gc,
        };

        epoch_gc_thread_register_local(&epoch_gc_thread);

        SECTION("true") {
            epoch_gc_thread.thread_terminated = true;
            REQUIRE(epoch_gc_thread_is_terminated(&epoch_gc_thread) == true);
        }

        SECTION("false") {
            epoch_gc_thread.thread_terminated = false;
            REQUIRE(epoch_gc_thread_is_terminated(&epoch_gc_thread) == false);
        }
    }

    SECTION("epoch_gc_thread_terminate") {
        epoch_gc_t epoch_gc = {
                .object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX,
        };

        epoch_gc_thread_t epoch_gc_thread = {
                .epoch_gc = &epoch_gc,
                .thread_terminated = false,
        };

        SECTION("terminate") {
            epoch_gc_thread_terminate(&epoch_gc_thread);
            REQUIRE(epoch_gc_thread_is_terminated(&epoch_gc_thread) == true);
        }
    }

    SECTION("epoch_gc_thread_advance_epoch") {
        epoch_gc_t epoch_gc = {
                .object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX,
        };

        epoch_gc_thread_t epoch_gc_thread = {
                .epoch = 0,
                .epoch_gc = &epoch_gc,
        };

        SECTION("advance once") {
            epoch_gc_thread_advance_epoch(&epoch_gc_thread);
            REQUIRE(epoch_gc_thread.epoch > 0);
        }

        SECTION("advance twice") {
            epoch_gc_thread_advance_epoch(&epoch_gc_thread);
            uint64_t epoch_temp = epoch_gc_thread.epoch;
            usleep(10000);
            epoch_gc_thread_advance_epoch(&epoch_gc_thread);
            REQUIRE(epoch_gc_thread.epoch > epoch_temp);
        }
    }

    SECTION("epoch_gc_thread_destruct_staged_objects_batch") {
        epoch_gc_staged_object_t *staged_objects[EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE];

        for(uint64_t i = 0; i < EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE; i++) {
            staged_objects[i] = (epoch_gc_staged_object_t*)ffma_mem_alloc(16);
            staged_objects[i]->object = (void*)i;
        }

        epoch_gc_thread_destruct_staged_objects_batch(
                test_epoch_gc_object_destructor_cb,
                EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE,
                staged_objects);

        REQUIRE(test_epoch_gc_object_destructor_cb_params_staged_objects_count ==
            EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE);
        REQUIRE(test_epoch_gc_object_destructor_cb_params_staged_objects ==
            staged_objects);
    }

    SECTION("epoch_gc_thread_collect") {
        epoch_gc_t epoch_gc = { 0 };
        epoch_gc.thread_list = double_linked_list_init();
        epoch_gc.object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX;
        spinlock_init(&epoch_gc.thread_list_spinlock);

        epoch_gc_thread_t *epoch_gc_thread = epoch_gc_thread_init(&epoch_gc);

        epoch_gc_thread_register_global(&epoch_gc, epoch_gc_thread);

        auto epoch_gc_staged_object_1 =
                (epoch_gc_staged_object_t*)ffma_mem_alloc(sizeof(epoch_gc_staged_object_t));
        auto epoch_gc_staged_object_2 =
                (epoch_gc_staged_object_t*)ffma_mem_alloc(sizeof(epoch_gc_staged_object_t));

        epoch_gc_staged_object_1->epoch = 100;
        epoch_gc_staged_object_1->object = (void*)1;

        epoch_gc_staged_object_2->epoch = 200;
        epoch_gc_staged_object_2->object = (void*)2;

        ring_bounded_spsc_enqueue(epoch_gc_thread->staged_objects_ring_last, epoch_gc_staged_object_1);
        ring_bounded_spsc_enqueue(epoch_gc_thread->staged_objects_ring_last, epoch_gc_staged_object_2);

        SECTION("two pointers, collect 2, no collection because of epoch") {

            epoch_gc_thread->epoch = 100;

            REQUIRE(epoch_gc_thread_collect(epoch_gc_thread, 2) == 0);
        }

        SECTION("two pointers, collect 2, 1 collected because of epoch") {
            epoch_gc_thread->epoch = 101;

            REQUIRE(epoch_gc_thread_collect(epoch_gc_thread, 2) == 1);

            epoch_gc_staged_object_1 = nullptr;
        }

        SECTION("two pointers, collect 2, 2 collected because of epoch") {
            epoch_gc_thread->epoch = 201;
            REQUIRE(epoch_gc_thread_collect(epoch_gc_thread, 2) == 2);

            epoch_gc_staged_object_1 = nullptr;
            epoch_gc_staged_object_2 = nullptr;
        }

        SECTION("two pointers, collect 2, 2 collected because of epoch in two rounds") {
            epoch_gc_thread->epoch = 201;
            REQUIRE(epoch_gc_thread_collect(epoch_gc_thread, 1) == 1);
            REQUIRE(epoch_gc_thread_collect(epoch_gc_thread, 1) == 1);

            epoch_gc_staged_object_1 = nullptr;
            epoch_gc_staged_object_2 = nullptr;
        }

        SECTION("test empty ring removal") {
            auto epoch_gc_staged_object_3 =
                    (epoch_gc_staged_object_t*)ffma_mem_alloc(sizeof(epoch_gc_staged_object_t));
            epoch_gc_staged_object_3->epoch = 300;
            epoch_gc_staged_object_3->object = (void*)1;

            epoch_gc_thread_append_new_staged_objects_ring(epoch_gc_thread);
            ring_bounded_spsc_t *ring_initial = epoch_gc_thread->staged_objects_ring_last;
            ring_bounded_spsc_enqueue(epoch_gc_thread->staged_objects_ring_last, epoch_gc_staged_object_3);

            epoch_gc_thread->epoch = 301;
            REQUIRE(epoch_gc_thread_collect(epoch_gc_thread, 1) == 1);
            REQUIRE(epoch_gc_thread_collect(epoch_gc_thread, 1) == 1);
            REQUIRE(epoch_gc_thread_collect(epoch_gc_thread, 1) == 1);
            REQUIRE(epoch_gc_thread->staged_objects_ring_last != ring_initial);
            REQUIRE(epoch_gc_thread->staged_objects_ring_list->count == 1);

            epoch_gc_staged_object_1 = nullptr;
            epoch_gc_staged_object_2 = nullptr;
        }

        if (epoch_gc_staged_object_1) {
            ffma_mem_free(epoch_gc_staged_object_1);
        }

        if (epoch_gc_staged_object_2) {
            ffma_mem_free(epoch_gc_staged_object_2);
        }

        epoch_gc_thread_unregister_global(epoch_gc_thread);
    }

    SECTION("epoch_gc_thread_collect_all") {
        epoch_gc_t epoch_gc = { 0 };
        epoch_gc.thread_list = double_linked_list_init();
        epoch_gc.object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX;
        spinlock_init(&epoch_gc.thread_list_spinlock);

        epoch_gc_thread_t *epoch_gc_thread = epoch_gc_thread_init(&epoch_gc);

        epoch_gc_thread_register_global(&epoch_gc, epoch_gc_thread);

        auto epoch_gc_staged_object_1 =
                (epoch_gc_staged_object_t*)ffma_mem_alloc(sizeof(epoch_gc_staged_object_t));
        auto epoch_gc_staged_object_2 =
                (epoch_gc_staged_object_t*)ffma_mem_alloc(sizeof(epoch_gc_staged_object_t));

        epoch_gc_staged_object_1->epoch = 100;
        epoch_gc_staged_object_1->object = (void*)1;

        epoch_gc_staged_object_2->epoch = 200;
        epoch_gc_staged_object_2->object = (void*)2;

        ring_bounded_spsc_enqueue(epoch_gc_thread->staged_objects_ring_last, epoch_gc_staged_object_1);
        ring_bounded_spsc_enqueue(epoch_gc_thread->staged_objects_ring_last, epoch_gc_staged_object_2);

        SECTION("two pointers, collect 2, no collection because of epoch") {

            epoch_gc_thread->epoch = 100;

            REQUIRE(epoch_gc_thread_collect_all(epoch_gc_thread) == 0);
        }

        SECTION("two pointers, collect 2, 1 collected because of epoch") {
            epoch_gc_thread->epoch = 101;

            REQUIRE(epoch_gc_thread_collect_all(epoch_gc_thread) == 1);

            epoch_gc_staged_object_1 = nullptr;
        }

        SECTION("two pointers, collect 2, 2 collected because of epoch") {
            epoch_gc_thread->epoch = 201;
            REQUIRE(epoch_gc_thread_collect_all(epoch_gc_thread) == 2);

            epoch_gc_staged_object_1 = nullptr;
            epoch_gc_staged_object_2 = nullptr;
        }

        SECTION("test empty ring removal") {
            auto epoch_gc_staged_object_3 =
                    (epoch_gc_staged_object_t*)ffma_mem_alloc(sizeof(epoch_gc_staged_object_t));
            epoch_gc_staged_object_3->epoch = 300;
            epoch_gc_staged_object_3->object = (void*)1;

            epoch_gc_thread_append_new_staged_objects_ring(epoch_gc_thread);
            ring_bounded_spsc_t *ring_initial = epoch_gc_thread->staged_objects_ring_last;
            ring_bounded_spsc_enqueue(epoch_gc_thread->staged_objects_ring_last, epoch_gc_staged_object_3);

            epoch_gc_thread->epoch = 301;
            REQUIRE(epoch_gc_thread_collect_all(epoch_gc_thread) == 3);
            REQUIRE(epoch_gc_thread->staged_objects_ring_last != ring_initial);
            REQUIRE(epoch_gc_thread->staged_objects_ring_list->count == 1);

            epoch_gc_staged_object_1 = nullptr;
            epoch_gc_staged_object_2 = nullptr;
        }

        if (epoch_gc_staged_object_1) {
            ffma_mem_free(epoch_gc_staged_object_1);
        }

        if (epoch_gc_staged_object_2) {
            ffma_mem_free(epoch_gc_staged_object_2);
        }

        epoch_gc_thread_unregister_global(epoch_gc_thread);
    }

    SECTION("epoch_gc_stage_object") {
        epoch_gc_register_object_type_destructor_cb(
                EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX,
                test_epoch_gc_object_destructor_cb);

        epoch_gc_t epoch_gc = { 0 };
        epoch_gc.thread_list = double_linked_list_init();
        epoch_gc.object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX;
        spinlock_init(&epoch_gc.thread_list_spinlock);

        epoch_gc_thread_t *epoch_gc_thread = epoch_gc_thread_init(&epoch_gc);

        epoch_gc_thread_register_global(&epoch_gc, epoch_gc_thread);
        epoch_gc_thread_register_local(epoch_gc_thread);

        SECTION("stage 1 object") {
            REQUIRE(epoch_gc_stage_object(EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX, (void*)1) == true);

            REQUIRE(ring_bounded_spsc_get_length(epoch_gc_thread->staged_objects_ring_last) == 1);
            REQUIRE(((epoch_gc_staged_object_t*)ring_bounded_spsc_dequeue(
                    epoch_gc_thread->staged_objects_ring_last))->object == (void*)1);
        }

        SECTION("stage 2 objects") {
            REQUIRE(epoch_gc_stage_object(EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX, (void*)1) == true);
            usleep(10000);
            REQUIRE(epoch_gc_stage_object(EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX, (void*)2) == true);

            REQUIRE(ring_bounded_spsc_get_length(epoch_gc_thread->staged_objects_ring_last) == 2);

            auto* staged_object_1 =
                    (epoch_gc_staged_object_t*)ring_bounded_spsc_dequeue(epoch_gc_thread->staged_objects_ring_last);
            auto* staged_object_2 =
                    (epoch_gc_staged_object_t*)ring_bounded_spsc_dequeue(epoch_gc_thread->staged_objects_ring_last);

            REQUIRE(staged_object_1->object == (void*)1);
            REQUIRE(staged_object_2->object == (void*)2);
            REQUIRE(staged_object_2->epoch > staged_object_1->epoch);
        }

        SECTION("fill one ring") {
            ring_bounded_spsc_t *ring_initial = epoch_gc_thread->staged_objects_ring_last;

            for(uint64_t i = 0; i < EPOCH_GC_STAGED_OBJECTS_RING_SIZE + 1; i++) {
                REQUIRE(epoch_gc_stage_object(
                        EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX, (void*)i) == true);
            }

            REQUIRE(ring_bounded_spsc_get_length(
                    (ring_bounded_spsc_t*)epoch_gc_thread->staged_objects_ring_list->head
                    ) == EPOCH_GC_STAGED_OBJECTS_RING_SIZE);
            REQUIRE(ring_bounded_spsc_get_length(
                    (ring_bounded_spsc_t*)epoch_gc_thread->staged_objects_ring_list->tail) == 1);

            REQUIRE(epoch_gc_thread->staged_objects_ring_last != ring_initial);
            REQUIRE(epoch_gc_thread->staged_objects_ring_list->count == 2);
        }

        // Free up the staged objects
        double_linked_list_item_t *item = epoch_gc_thread->staged_objects_ring_list->head;
        while(item != nullptr) {
            double_linked_list_item_t *current = item;
            auto *ring = (ring_bounded_spsc_t*)current->data;
            item = item->next;

            // When freeing the epoch_gc_thread structure there should NEVER be staged objects in the ring
            epoch_gc_staged_object_t *staged_object = nullptr;
            while((staged_object = (epoch_gc_staged_object_t*)ring_bounded_spsc_dequeue(ring)) != nullptr) {
                ffma_mem_free(staged_object);
            }

            ring_bounded_spsc_free(ring);
            double_linked_list_item_free(current);
        }

        double_linked_list_free(epoch_gc_thread->staged_objects_ring_list);

        epoch_gc_thread_unregister_global(epoch_gc_thread);

        epoch_gc_staged_object_destructor_cb[EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX] = nullptr;
    }
}
