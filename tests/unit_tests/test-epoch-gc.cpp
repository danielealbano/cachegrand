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
#include <pthread.h>

#include "clock.h"
#include "random.h"
#include "spinlock.h"
#include "xalloc.h"
#include "utils_cpu.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"

#include "epoch_gc.h"

void test_epoch_gc_object_destructor_cb_test(
        uint8_t staged_objects_count,
        epoch_gc_staged_object_t staged_objects[EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE]) {
    for(uint64_t i = 0; i < staged_objects_count; i++) {
        REQUIRE((uintptr_t)staged_objects[i].data.object == (uintptr_t)(i+1));
    }
}

void test_epoch_gc_object_destructor_cb_real(
        uint8_t staged_objects_count,
        epoch_gc_staged_object_t staged_objects[EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE]) {
    for(uint64_t i = 0; i < staged_objects_count; i++) {
        free(staged_objects[i].data.object);
    }
}

typedef struct test_epoch_gc_fuzzy_separated_producer_consumer_thread_data
    test_epoch_gc_fuzzy_separated_producer_consumer_thread_data_t;
struct test_epoch_gc_fuzzy_separated_producer_consumer_thread_data {
    bool terminate;
    epoch_gc_t *epoch_gc;
    uint32_t producers_count;
    uint32_t producers_terminated;
    uint64_t staged_objects_counter;
    uint64_t advanced_epochs_counter;
    uint64_t freed_objects_counter;
};

void *test_epoch_gc_fuzzy_separated_producer_consumer_producer_thread_func(
        void *user_data) {
    auto thread_data = (test_epoch_gc_fuzzy_separated_producer_consumer_thread_data_t*)user_data;

    epoch_gc_thread_t *epoch_gc_thread = epoch_gc_thread_init();

    epoch_gc_thread_register_global(thread_data->epoch_gc, epoch_gc_thread);
    epoch_gc_thread_register_local(epoch_gc_thread);

    do {
        usleep(random_generate() % 5);

        void *mem_ptr = malloc(8);
        REQUIRE(epoch_gc_stage_object(epoch_gc_thread->epoch_gc->object_type, mem_ptr));
        __atomic_fetch_add(&thread_data->staged_objects_counter, 1, __ATOMIC_ACQ_REL);

        if (random_generate() % 1000 > 500) {
            __atomic_fetch_add(&thread_data->advanced_epochs_counter, 1, __ATOMIC_ACQ_REL);
            epoch_gc_thread_advance_epoch_tsc(epoch_gc_thread);
        }

        MEMORY_FENCE_STORE();

        MEMORY_FENCE_LOAD();
    } while(thread_data->terminate == false);

    epoch_gc_thread_terminate(epoch_gc_thread);
    epoch_gc_thread_unregister_local(epoch_gc_thread);

    __atomic_fetch_add(&thread_data->producers_terminated, 1, __ATOMIC_ACQ_REL);

    return nullptr;
}

void *test_epoch_gc_fuzzy_separated_producer_consumer_consumer_thread_func(
        void *user_data) {
     auto thread_data = (test_epoch_gc_fuzzy_separated_producer_consumer_thread_data_t*)user_data;
     epoch_gc_t *epoch_gc = thread_data->epoch_gc;
    epoch_gc_thread_t **epoch_gc_thread_list_cache = nullptr;
    uint32_t epoch_gc_thread_list_length = 0;
    uint32_t epoch_gc_thread_list_index = 0;
    uint64_t epoch_gc_thread_list_change_epoch = 0;

    do {
        // Sleep 5ms
        usleep(5 * 1000);

        // Check if the cache of threads for the epoch_gc has to be rebuilt
        MEMORY_FENCE_LOAD();
        if (epoch_gc_thread_list_change_epoch == 0 ||
            epoch_gc_thread_list_change_epoch != epoch_gc->thread_list_change_epoch) {
            if (epoch_gc_thread_list_cache != nullptr) {
                free(epoch_gc_thread_list_cache);
                epoch_gc_thread_list_length = 0;
            }

            // Lock the thread list
            spinlock_lock(&epoch_gc->thread_list_spinlock);

            epoch_gc_thread_list_length = epoch_gc->thread_list->count;
            epoch_gc_thread_list_cache = (epoch_gc_thread_t**)malloc(
                    epoch_gc_thread_list_length * sizeof(epoch_gc_thread_t*));

            epoch_gc_thread_list_index = 0;
            double_linked_list_item_t *item = nullptr;
            while((item = double_linked_list_iter_next(epoch_gc->thread_list, item)) != nullptr) {
                epoch_gc_thread_list_cache[epoch_gc_thread_list_index] = (epoch_gc_thread_t*)item->data;
                epoch_gc_thread_list_index++;
            }

            epoch_gc_thread_list_change_epoch = epoch_gc->thread_list_change_epoch;

            // Unlock the thread list
            spinlock_unlock(&epoch_gc->thread_list_spinlock);
        }

        // Iterate over the cached epoch gc threads
        for(
                epoch_gc_thread_list_index = 0;
                epoch_gc_thread_list_index < epoch_gc_thread_list_length;
                epoch_gc_thread_list_index++) {
            thread_data->freed_objects_counter += epoch_gc_thread_collect_all(
                    epoch_gc_thread_list_cache[epoch_gc_thread_list_index]);
            MEMORY_FENCE_STORE();
        }

        MEMORY_FENCE_LOAD();
    } while(!thread_data->terminate);

    // Wait for all the threads to be marked as terminated
    bool epoch_gc_all_terminated;
    do {
        epoch_gc_all_terminated = true;
        for(
                epoch_gc_thread_list_index = 0;
                epoch_gc_thread_list_index < epoch_gc_thread_list_length;
                epoch_gc_thread_list_index++) {
            if (!epoch_gc_thread_is_terminated(epoch_gc_thread_list_cache[epoch_gc_thread_list_index])) {
                epoch_gc_all_terminated = false;
                break;
            }
        }
    } while(!epoch_gc_all_terminated);

    REQUIRE(thread_data->producers_count == thread_data->producers_terminated);

    // All the threads are terminated, advance the epoch on the epoch_gc_thread, trigger a collect_all and at the end
    // free the structure
    for(
            epoch_gc_thread_list_index = 0;
            epoch_gc_thread_list_index < epoch_gc_thread_list_length;
            epoch_gc_thread_list_index++) {
        epoch_gc_thread_advance_epoch_tsc(epoch_gc_thread_list_cache[epoch_gc_thread_list_index]);

        thread_data->freed_objects_counter += epoch_gc_thread_collect_all(
                epoch_gc_thread_list_cache[epoch_gc_thread_list_index]);
    }

    MEMORY_FENCE_LOAD_STORE();

    for(
            epoch_gc_thread_list_index = 0;
            epoch_gc_thread_list_index < epoch_gc_thread_list_length;
            epoch_gc_thread_list_index++) {
        epoch_gc_thread_unregister_global(epoch_gc_thread_list_cache[epoch_gc_thread_list_index]);
        epoch_gc_thread_free(epoch_gc_thread_list_cache[epoch_gc_thread_list_index]);
    }

    epoch_gc_unregister_object_type_destructor_cb(EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE);

    if (epoch_gc_thread_list_cache != nullptr) {
        free(epoch_gc_thread_list_cache);
        epoch_gc_thread_list_length = 0;
    }

    return nullptr;
}

void test_epoch_gc_fuzzy_separated_producers_consumer(
        epoch_gc_t *epoch_gc,
        uint64_t duration,
        uint32_t producers_count) {
    timespec_t start_time, current_time, diff_time;
    pthread_t *pthread_producers;
    pthread_t pthread_consumer;
    pthread_attr_t attr;

    test_epoch_gc_fuzzy_separated_producer_consumer_thread_data_t thread_data = {
            .terminate = false,
            .epoch_gc = epoch_gc,
            .producers_count = 0,
            .producers_terminated = 0,
            .staged_objects_counter = 0,
            .advanced_epochs_counter = 0,
            .freed_objects_counter = 0,
    };

    REQUIRE(pthread_attr_init(&attr) == 0);

    // Start the requested producers
    thread_data.producers_count = producers_count;
    pthread_producers = (pthread_t*)malloc(producers_count * sizeof(pthread_t));
    for(uint32_t i = 0; i < producers_count; i++) {
        REQUIRE(pthread_create(
                &pthread_producers[i],
                &attr,
                &test_epoch_gc_fuzzy_separated_producer_consumer_producer_thread_func,
                (void *)&thread_data) == 0);
    }

    // Start the consumer
    REQUIRE(pthread_create(
            &pthread_consumer,
            &attr,
            &test_epoch_gc_fuzzy_separated_producer_consumer_consumer_thread_func,
            (void *)&thread_data) == 0);

    clock_monotonic(&start_time);

    do {
        clock_monotonic(&current_time);
        sched_yield();

        clock_diff(&start_time, &current_time, &diff_time);
    } while(diff_time.tv_sec < duration);

    thread_data.terminate = true;
    MEMORY_FENCE_STORE();

    // The producers will terminate before the consumer
    for(uint32_t i = 0; i < producers_count; i++) {
        pthread_join(pthread_producers[i], nullptr);
    }
    pthread_join(pthread_consumer, nullptr);

    free(pthread_producers);

    REQUIRE(thread_data.staged_objects_counter == thread_data.freed_objects_counter);
}

TEST_CASE("epoch_gc.c", "[epoch_gc]") {
    SECTION("epoch_gc_init") {
        SECTION("valid object type") {
            epoch_gc_t *epoch_gc = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE);

            REQUIRE(epoch_gc->object_type == EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE);
            REQUIRE(spinlock_is_locked(&epoch_gc->thread_list_spinlock) == false);
            REQUIRE(epoch_gc->thread_list != nullptr);

            double_linked_list_free(epoch_gc->thread_list);
            xalloc_free(epoch_gc);
        }
    }

#if DEBUG == 1
    SECTION("epoch_gc_register_object_type_destructor_cb") {
        SECTION("valid object type and valid function pointer") {
            epoch_gc_register_object_type_destructor_cb(
                    EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE,
                    test_epoch_gc_object_destructor_cb_test);

            REQUIRE(epoch_gc_get_epoch_gc_staged_object_destructor_cb()[EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE] ==
                    test_epoch_gc_object_destructor_cb_test);

            epoch_gc_unregister_object_type_destructor_cb(EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE);
        }
    }
#endif

    SECTION("epoch_gc_thread_append_new_staged_objects_ring") {
        epoch_gc_thread_t epoch_gc_thread = { nullptr };
        epoch_gc_thread.staged_objects_ring_list = double_linked_list_init();

        SECTION("append 1 new ring") {
            epoch_gc_thread_append_new_staged_objects_ring(&epoch_gc_thread);

            REQUIRE(epoch_gc_thread.staged_objects_ring_list->count == 1);
            REQUIRE(epoch_gc_thread.staged_objects_ring_list->tail->data == epoch_gc_thread.staged_objects_ring_last);
            REQUIRE(ring_bounded_queue_spsc_uint128_get_length(epoch_gc_thread.staged_objects_ring_last) == 0);
        }

        SECTION("append 2 new rings") {
            epoch_gc_thread_append_new_staged_objects_ring(&epoch_gc_thread);
            epoch_gc_thread_append_new_staged_objects_ring(&epoch_gc_thread);

            REQUIRE(epoch_gc_thread.staged_objects_ring_list->count == 2);
            REQUIRE(epoch_gc_thread.staged_objects_ring_list->tail->data == epoch_gc_thread.staged_objects_ring_last);
            REQUIRE(ring_bounded_queue_spsc_uint128_get_length(epoch_gc_thread.staged_objects_ring_last) == 0);
        }

        SECTION("append multiple new rings") {
            for(int i = 0; i < 64; i++) {
                epoch_gc_thread_append_new_staged_objects_ring(&epoch_gc_thread);
            }

            REQUIRE(epoch_gc_thread.staged_objects_ring_list->count == 64);
            REQUIRE(epoch_gc_thread.staged_objects_ring_list->tail->data == epoch_gc_thread.staged_objects_ring_last);
            REQUIRE(ring_bounded_queue_spsc_uint128_get_length(epoch_gc_thread.staged_objects_ring_last) == 0);
        }

        double_linked_list_item_t *rb_item = epoch_gc_thread.staged_objects_ring_list->head;
        while(rb_item != nullptr) {
            double_linked_list_item_t *current = rb_item;
            rb_item = rb_item->next;

            ring_bounded_queue_spsc_uint128_free((ring_bounded_queue_spsc_uint128_t*)current->data);
            double_linked_list_item_free(current);
        }

        double_linked_list_free(epoch_gc_thread.staged_objects_ring_list);
    }

    SECTION("epoch_gc_thread_init") {
        epoch_gc_t epoch_gc = { nullptr };
        epoch_gc.thread_list = double_linked_list_init();
        epoch_gc.object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE;
        spinlock_init(&epoch_gc.thread_list_spinlock);

        SECTION("create one thread") {
            auto epoch_gc_thread = epoch_gc_thread_init();

            REQUIRE(epoch_gc_thread != nullptr);
            REQUIRE(epoch_gc_thread->epoch == 0);
            REQUIRE(epoch_gc_thread->staged_objects_ring_list->count == 1);
            REQUIRE(epoch_gc_thread->staged_objects_ring_list->tail->data == epoch_gc_thread->staged_objects_ring_last);
            REQUIRE(ring_bounded_queue_spsc_uint128_get_length(epoch_gc_thread->staged_objects_ring_last) == 0);

            epoch_gc_thread_free(epoch_gc_thread);
        }

        // Free the thread list
        double_linked_list_free(epoch_gc.thread_list);
    }

    SECTION("epoch_gc_thread_register_global") {
        epoch_gc_t epoch_gc = {nullptr };
        epoch_gc.thread_list = double_linked_list_init();
        epoch_gc.object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE;
        spinlock_init(&epoch_gc.thread_list_spinlock);

        epoch_gc_thread_t *epoch_gc_thread_1 = epoch_gc_thread_init();
        epoch_gc_thread_t *epoch_gc_thread_2 = epoch_gc_thread_init();
        epoch_gc_thread_t *epoch_gc_thread_3 = epoch_gc_thread_init();

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

        // Destroy the list of items
        double_linked_list_item_t *item = epoch_gc.thread_list->head;
        while(item != nullptr) {
            double_linked_list_item_t *current = item;
            item = item->next;

            double_linked_list_item_free(current);
        }

        // Free the thread list
        double_linked_list_free(epoch_gc.thread_list);

        epoch_gc_thread_free(epoch_gc_thread_1);
        epoch_gc_thread_free(epoch_gc_thread_2);
        epoch_gc_thread_free(epoch_gc_thread_3);
    }

#if DEBUG == 1
    SECTION("epoch_gc_thread_register_local") {
        epoch_gc_t epoch_gc = {
                .object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE,
        };

        epoch_gc_thread_t epoch_gc_thread = {
                .epoch_gc = &epoch_gc,
        };

        epoch_gc_thread_register_local(&epoch_gc_thread);

        REQUIRE(epoch_gc_get_thread_local_epoch_gc()[epoch_gc.object_type] == &epoch_gc_thread);

        epoch_gc_thread_unregister_local(&epoch_gc_thread);
    }
#endif

    SECTION("epoch_gc_thread_get_instance") {
        epoch_gc_t epoch_gc = {
                .object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE,
        };

        epoch_gc_thread_t epoch_gc_thread = {
                .epoch_gc = &epoch_gc,
        };

        epoch_gc_thread_register_local(&epoch_gc_thread);

        epoch_gc_t *epoch_gc_new = nullptr;
        epoch_gc_thread_t *epoch_gc_thread_new = nullptr;

        epoch_gc_thread_get_instance(
                EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE,
                &epoch_gc_new,
                &epoch_gc_thread_new);

        REQUIRE(&epoch_gc == epoch_gc_new);
        REQUIRE(&epoch_gc_thread == epoch_gc_thread_new);

        epoch_gc_thread_unregister_local(&epoch_gc_thread);
    }

    SECTION("epoch_gc_thread_is_terminated") {
        epoch_gc_t epoch_gc = {
                .object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE,
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

        epoch_gc_thread_unregister_local(&epoch_gc_thread);
    }

    SECTION("epoch_gc_thread_advance_epoch_tsc") {
        epoch_gc_t epoch_gc = {
                .object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE,
        };

        epoch_gc_thread_t epoch_gc_thread = {
                .epoch = 0,
                .epoch_gc = &epoch_gc,
        };

        SECTION("advance once") {
            epoch_gc_thread_advance_epoch_tsc(&epoch_gc_thread);
            REQUIRE(epoch_gc_thread.epoch > 0);
        }

        SECTION("advance twice") {
            epoch_gc_thread_advance_epoch_tsc(&epoch_gc_thread);
            uint64_t epoch_temp = epoch_gc_thread.epoch;
            usleep(10000);
            epoch_gc_thread_advance_epoch_tsc(&epoch_gc_thread);
            REQUIRE(epoch_gc_thread.epoch > epoch_temp);
        }
    }

    SECTION("epoch_gc_thread_advance_epoch_by_one") {
        epoch_gc_t epoch_gc = {
                .object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE,
        };

        epoch_gc_thread_t epoch_gc_thread = {
                .epoch = 0,
                .epoch_gc = &epoch_gc,
        };

        SECTION("advance once") {
            epoch_gc_thread_advance_epoch_by_one(&epoch_gc_thread);
            REQUIRE(epoch_gc_thread.epoch == 1);
        }

        SECTION("advance twice") {
            epoch_gc_thread_advance_epoch_by_one(&epoch_gc_thread);
            epoch_gc_thread_advance_epoch_by_one(&epoch_gc_thread);
            REQUIRE(epoch_gc_thread.epoch == 2);
        }
    }

    SECTION("epoch_gc_thread_collect") {
        epoch_gc_t epoch_gc = { nullptr };
        epoch_gc.thread_list = double_linked_list_init();
        epoch_gc.object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE;
        spinlock_init(&epoch_gc.thread_list_spinlock);

        epoch_gc_thread_t *epoch_gc_thread = epoch_gc_thread_init();

        epoch_gc_register_object_type_destructor_cb(
                EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE, test_epoch_gc_object_destructor_cb_test);
        epoch_gc_thread_register_global(&epoch_gc, epoch_gc_thread);

        epoch_gc_staged_object_t epoch_gc_staged_object_1 = { .data = { .epoch = 100, .object = (void*)1, } };
        epoch_gc_staged_object_t epoch_gc_staged_object_2 = { .data = { .epoch = 200, .object = (void*)2, } };

        ring_bounded_queue_spsc_uint128_enqueue(epoch_gc_thread->staged_objects_ring_last, epoch_gc_staged_object_1._packed);
        ring_bounded_queue_spsc_uint128_enqueue(epoch_gc_thread->staged_objects_ring_last, epoch_gc_staged_object_2._packed);

        SECTION("two pointers, collect 2, no collection because of epoch") {
            epoch_gc_thread->epoch = 100;

            REQUIRE(epoch_gc_thread_collect(epoch_gc_thread, 2) == 0);
        }

        SECTION("two pointers, collect 2, 1 collected because of epoch") {
            epoch_gc_thread->epoch = 101;

            REQUIRE(epoch_gc_thread_collect(epoch_gc_thread, 2) == 1);
        }

        SECTION("two pointers, collect 2, 2 collected because of epoch") {
            epoch_gc_thread->epoch = 201;
            REQUIRE(epoch_gc_thread_collect(epoch_gc_thread, 2) == 2);
        }

        SECTION("two pointers, collect 2, 2 collected because of epoch in two rounds because of max_objects") {
            epoch_gc_thread->epoch = 201;

            REQUIRE(epoch_gc_thread_collect(epoch_gc_thread, 1) == 1);

            // The test destructor expects each item of the batch be numbered from 1 to 16, as the staged objects are
            // being purged in two rounds both the pointers must have value 1
            ring_bounded_queue_spsc_uint128_dequeue(epoch_gc_thread->staged_objects_ring_last, nullptr);
            epoch_gc_staged_object_2.data.object = (void*)1;
            ring_bounded_queue_spsc_uint128_enqueue(epoch_gc_thread->staged_objects_ring_last, epoch_gc_staged_object_2._packed);

            REQUIRE(epoch_gc_thread_collect(epoch_gc_thread, 1) == 1);
        }

        SECTION("test empty ring removal") {
            epoch_gc_staged_object_t epoch_gc_staged_object_3 = { .data = { .epoch = 300, .object = (void*)3, } };

            // Get the current ring
            ring_bounded_queue_spsc_uint128_t *ring_initial = epoch_gc_thread->staged_objects_ring_last;

            // Append a new ring
            epoch_gc_thread_append_new_staged_objects_ring(epoch_gc_thread);

            // Enqueue the item onto the new ring
            ring_bounded_queue_spsc_uint128_enqueue(
                    epoch_gc_thread->staged_objects_ring_last,
                    epoch_gc_staged_object_3._packed);

            // Set the epoch in a way that all the staged pointers will get deleted
            epoch_gc_thread->epoch = 301;

            REQUIRE(epoch_gc_thread_collect(epoch_gc_thread, 3) == 3);
            REQUIRE(epoch_gc_thread->staged_objects_ring_last != ring_initial);
            REQUIRE(epoch_gc_thread->staged_objects_ring_list->count == 1);
        }

        bool found = false;
        do {
            epoch_gc_staged_object_t staged_object_temp;
            staged_object_temp._packed = ring_bounded_queue_spsc_uint128_dequeue(
                    epoch_gc_thread->staged_objects_ring_last, &found);

        } while(found);

        epoch_gc_thread_unregister_global(epoch_gc_thread);
        epoch_gc_unregister_object_type_destructor_cb(EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE);
        epoch_gc_thread_free(epoch_gc_thread);
    }

    SECTION("epoch_gc_thread_collect_all") {
        epoch_gc_t epoch_gc = { nullptr };
        epoch_gc.thread_list = double_linked_list_init();
        epoch_gc.object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE;
        spinlock_init(&epoch_gc.thread_list_spinlock);

        epoch_gc_thread_t *epoch_gc_thread = epoch_gc_thread_init();

        epoch_gc_register_object_type_destructor_cb(
                EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE,
                test_epoch_gc_object_destructor_cb_test);
        epoch_gc_thread_register_global(&epoch_gc, epoch_gc_thread);

        epoch_gc_staged_object_t epoch_gc_staged_object_1 = { .data = { .epoch = 100, .object = (void*)1, } };
        epoch_gc_staged_object_t epoch_gc_staged_object_2 = { .data = { .epoch = 200, .object = (void*)2, } };

        ring_bounded_queue_spsc_uint128_enqueue(epoch_gc_thread->staged_objects_ring_last, epoch_gc_staged_object_1._packed);
        ring_bounded_queue_spsc_uint128_enqueue(epoch_gc_thread->staged_objects_ring_last, epoch_gc_staged_object_2._packed);

        SECTION("two pointers, collect 2, no collection because of epoch") {
            epoch_gc_thread->epoch = 100;

            REQUIRE(epoch_gc_thread_collect_all(epoch_gc_thread) == 0);
        }

        SECTION("two pointers, collect 2, 1 collected because of epoch") {
            epoch_gc_thread->epoch = 101;

            REQUIRE(epoch_gc_thread_collect_all(epoch_gc_thread) == 1);
        }

        SECTION("two pointers, collect 2, 2 collected because of epoch") {
            epoch_gc_thread->epoch = 201;
            REQUIRE(epoch_gc_thread_collect_all(epoch_gc_thread) == 2);
        }

        SECTION("test empty ring removal") {
            epoch_gc_staged_object_t epoch_gc_staged_object_3 = { .data = { .epoch = 300, .object = (void*)3, } };

            // Get the current ring
            ring_bounded_queue_spsc_uint128_t *ring_initial = epoch_gc_thread->staged_objects_ring_last;

            // Append a new ring
            epoch_gc_thread_append_new_staged_objects_ring(epoch_gc_thread);

            // Enqueue the item onto the new ring
            ring_bounded_queue_spsc_uint128_enqueue(
                    epoch_gc_thread->staged_objects_ring_last,
                    epoch_gc_staged_object_3._packed);

            // Set the epoch in a way that all the staged pointers will get deleted
            epoch_gc_thread->epoch = 301;

            REQUIRE(epoch_gc_thread_collect(epoch_gc_thread, 3) == 3);
            REQUIRE(epoch_gc_thread->staged_objects_ring_last != ring_initial);
            REQUIRE(epoch_gc_thread->staged_objects_ring_list->count == 1);
        }

        bool found = false;
        do {
            epoch_gc_staged_object_t staged_object_temp;
            staged_object_temp._packed = ring_bounded_queue_spsc_uint128_dequeue(
                    epoch_gc_thread->staged_objects_ring_last, &found);

        } while(found);

        epoch_gc_thread_unregister_global(epoch_gc_thread);
        epoch_gc_unregister_object_type_destructor_cb(EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE);
        epoch_gc_thread_free(epoch_gc_thread);
    }

    SECTION("epoch_gc_thread_terminate") {
        epoch_gc_t epoch_gc = { nullptr };
        epoch_gc.thread_list = double_linked_list_init();
        epoch_gc.object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE;
        spinlock_init(&epoch_gc.thread_list_spinlock);

        epoch_gc_thread_t *epoch_gc_thread = epoch_gc_thread_init();

        epoch_gc_register_object_type_destructor_cb(
                EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE,
                test_epoch_gc_object_destructor_cb_test);
        epoch_gc_thread_register_global(&epoch_gc, epoch_gc_thread);

        SECTION("terminate") {
            epoch_gc_thread_terminate(epoch_gc_thread);
            REQUIRE(epoch_gc_thread_is_terminated(epoch_gc_thread) == true);
        }

        epoch_gc_thread_unregister_global(epoch_gc_thread);
        epoch_gc_unregister_object_type_destructor_cb(EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE);
        epoch_gc_thread_free(epoch_gc_thread);
    }

    SECTION("epoch_gc_stage_object") {
        epoch_gc_register_object_type_destructor_cb(
                EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE,
                test_epoch_gc_object_destructor_cb_test);

        epoch_gc_t epoch_gc = { nullptr };
        epoch_gc.thread_list = double_linked_list_init();
        epoch_gc.object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE;
        spinlock_init(&epoch_gc.thread_list_spinlock);

        epoch_gc_thread_t *epoch_gc_thread = epoch_gc_thread_init();

        epoch_gc_thread_register_global(&epoch_gc, epoch_gc_thread);
        epoch_gc_thread_register_local(epoch_gc_thread);

        SECTION("stage 1 object") {
            REQUIRE(epoch_gc_stage_object(
                    EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE,
                    (void*)1) == true);

            REQUIRE(ring_bounded_queue_spsc_uint128_get_length(epoch_gc_thread->staged_objects_ring_last) == 1);

            epoch_gc_staged_object_t staged_object;
            staged_object._packed =
                    ring_bounded_queue_spsc_uint128_dequeue(epoch_gc_thread->staged_objects_ring_last, nullptr);

            REQUIRE(staged_object.data.object == (void*)1);
            REQUIRE(staged_object.data.epoch == epoch_gc_thread->epoch);
        }

        SECTION("stage 2 objects") {
            REQUIRE(epoch_gc_stage_object(
                    EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE,
                    (void*)1) == true);
            usleep(10000);
            REQUIRE(epoch_gc_stage_object(
                    EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE,
                    (void*)2) == true);

            REQUIRE(ring_bounded_queue_spsc_uint128_get_length(epoch_gc_thread->staged_objects_ring_last) == 2);

            epoch_gc_staged_object_t staged_object_1, staged_object_2;
            staged_object_1._packed =
                    ring_bounded_queue_spsc_uint128_dequeue(epoch_gc_thread->staged_objects_ring_last, nullptr);
            staged_object_2._packed =
                    ring_bounded_queue_spsc_uint128_dequeue(epoch_gc_thread->staged_objects_ring_last, nullptr);

            REQUIRE(staged_object_1.data.object == (void*)1);
            REQUIRE(staged_object_2.data.object == (void*)2);
            REQUIRE(staged_object_1.data.epoch == epoch_gc_thread->epoch);
            REQUIRE(staged_object_2.data.epoch == epoch_gc_thread->epoch);
        }

        SECTION("fill one ring") {
            ring_bounded_queue_spsc_uint128_t *ring_initial = epoch_gc_thread->staged_objects_ring_last;

            for(uint64_t i = 0; i < EPOCH_GC_STAGED_OBJECTS_RING_SIZE + 1; i++) {
                REQUIRE(epoch_gc_stage_object(
                        EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE, (void*)i) == true);
            }

            REQUIRE(ring_bounded_queue_spsc_uint128_get_length(
                    (ring_bounded_queue_spsc_uint128_t*)epoch_gc_thread->staged_objects_ring_list->head->data)
                    == EPOCH_GC_STAGED_OBJECTS_RING_SIZE);
            REQUIRE(ring_bounded_queue_spsc_uint128_get_length(
                    (ring_bounded_queue_spsc_uint128_t*)epoch_gc_thread->staged_objects_ring_list->tail->data)
                    == 1);

            REQUIRE(epoch_gc_thread->staged_objects_ring_last != ring_initial);
            REQUIRE(epoch_gc_thread->staged_objects_ring_list->count == 2);
        }

        bool found = false;
        do {
            epoch_gc_staged_object_t staged_object_temp;
            staged_object_temp._packed = ring_bounded_queue_spsc_uint128_dequeue(
                    (ring_bounded_queue_spsc_uint128_t*)epoch_gc_thread->staged_objects_ring_list->head->data,
                    &found);
        } while(found);

        if (epoch_gc_thread->staged_objects_ring_list->head->next) {
            do {
                epoch_gc_staged_object_t staged_object_temp;
                staged_object_temp._packed = ring_bounded_queue_spsc_uint128_dequeue(
                        (ring_bounded_queue_spsc_uint128_t*)epoch_gc_thread->staged_objects_ring_list->head->next->data,
                        &found);
            } while(found);
        }

        epoch_gc_thread_unregister_local(epoch_gc_thread);
        epoch_gc_thread_unregister_global(epoch_gc_thread);
        epoch_gc_unregister_object_type_destructor_cb(EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE);
        epoch_gc_thread_free(epoch_gc_thread);
    }

    SECTION("test workflow end to end") {
        epoch_gc_register_object_type_destructor_cb(
                EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE,
                test_epoch_gc_object_destructor_cb_test);

        epoch_gc_t epoch_gc = { nullptr };
        epoch_gc.thread_list = double_linked_list_init();
        epoch_gc.object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE;
        spinlock_init(&epoch_gc.thread_list_spinlock);

        epoch_gc_thread_t *epoch_gc_thread = epoch_gc_thread_init();

        epoch_gc_thread_register_global(&epoch_gc, epoch_gc_thread);
        epoch_gc_thread_register_local(epoch_gc_thread);

        // Stage 2 object
        REQUIRE(epoch_gc_stage_object(
                EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE, (void*)1) == true);
        REQUIRE(epoch_gc_stage_object(
                EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE, (void*)2) == true);

        // Try to collect but nothing to be collected
        REQUIRE(epoch_gc_thread_collect_all(epoch_gc_thread) == 0);

        // Advance the epoch gc thread epoch
        epoch_gc_thread_advance_epoch_tsc(epoch_gc_thread);

        // Stage a third object
        REQUIRE(epoch_gc_stage_object(
                EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE, (void*)1) == true);

        // Collect the first 2 objects
        REQUIRE(epoch_gc_thread_collect_all(epoch_gc_thread) == 2);

        // Advance the epoch gc thread epoch
        epoch_gc_thread_advance_epoch_tsc(epoch_gc_thread);

        // Collect the third object
        REQUIRE(epoch_gc_thread_collect_all(epoch_gc_thread) == 1);

        epoch_gc_thread_terminate(epoch_gc_thread);
        epoch_gc_thread_unregister_local(epoch_gc_thread);
        epoch_gc_thread_unregister_global(epoch_gc_thread);
        epoch_gc_thread_free(epoch_gc_thread);
        epoch_gc_unregister_object_type_destructor_cb(EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE);

        double_linked_list_free(epoch_gc.thread_list);
    }

    SECTION("fuzzy staging/collecting") {
        epoch_gc_register_object_type_destructor_cb(
                EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE,
                test_epoch_gc_object_destructor_cb_real);

        epoch_gc_t *epoch_gc = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE);

        SECTION("one producer / one consumer") {
            test_epoch_gc_fuzzy_separated_producers_consumer(
                    epoch_gc,
                    2,
                    1);
        }

        SECTION("multiple producers / one consumer") {
            test_epoch_gc_fuzzy_separated_producers_consumer(
                    epoch_gc,
                    2,
                    utils_cpu_count());
        }

        epoch_gc_free(epoch_gc);
    }
}
