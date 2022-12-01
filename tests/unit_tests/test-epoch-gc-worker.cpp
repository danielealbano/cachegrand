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
#include <cstring>
#include <pthread.h>

#include "clock.h"
#include "random.h"
#include "spinlock.h"
#include "xalloc.h"
#include "utils_cpu.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "epoch_gc.h"

#include "epoch_gc_worker.h"

void test_epoch_gc_worker_object_destructor_cb_real(
        uint8_t staged_objects_count,
        epoch_gc_staged_object_t staged_objects[EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE]) {
    for(uint64_t i = 0; i < staged_objects_count; i++) {
        free(staged_objects[i].data.object);
    }
}

typedef struct test_epoch_gc_worker_fuzzy_separated_producer_consumer_thread_data
    test_epoch_gc_worker_fuzzy_separated_producer_consumer_thread_data_t;
struct test_epoch_gc_worker_fuzzy_separated_producer_consumer_thread_data {
    bool *terminate;
    epoch_gc_t *epoch_gc;
    uint64_t staged_objects_counter;
};

void *test_epoch_gc_worker_fuzzy_separated_producer_consumer_producer_thread_func(
        void *user_data) {
    auto thread_data = (test_epoch_gc_worker_fuzzy_separated_producer_consumer_thread_data_t*)user_data;

    epoch_gc_thread_t *epoch_gc_thread = epoch_gc_thread_init();

    epoch_gc_thread_register_global(thread_data->epoch_gc, epoch_gc_thread);
    epoch_gc_thread_register_local(epoch_gc_thread);

    do {
        usleep(random_generate() % 5);

        void *mem_ptr = malloc(8);
        REQUIRE(epoch_gc_stage_object(epoch_gc_thread->epoch_gc->object_type, mem_ptr));
        __atomic_fetch_add(&thread_data->staged_objects_counter, 1, __ATOMIC_ACQ_REL);

        if (random_generate() % 1000 > 500) {
            epoch_gc_thread_advance_epoch_tsc(epoch_gc_thread);
        }

        MEMORY_FENCE_LOAD();
    } while(*thread_data->terminate == false);

    epoch_gc_thread_terminate(epoch_gc_thread);
    epoch_gc_thread_unregister_local(epoch_gc_thread);

    return nullptr;
}

void test_epoch_gc_worker_fuzzy_separated_producers_consumer(
        epoch_gc_t *epoch_gc,
        uint64_t duration,
        uint32_t producers_count) {
    timespec_t start_time, current_time, diff_time;
    pthread_t *pthread_producers;
    pthread_attr_t attr;

    bool terminate = false;
    test_epoch_gc_worker_fuzzy_separated_producer_consumer_thread_data_t thread_data = {
            .terminate = &terminate,
            .epoch_gc = epoch_gc,
            .staged_objects_counter = 0,
    };
    epoch_gc_worker_context_t epoch_gc_worker_context = {
            .epoch_gc = epoch_gc,
            .terminate_event_loop = &terminate,
            .stats = {
                .collected_objects = 0,
            }
    };

    REQUIRE(pthread_attr_init(&attr) == 0);

    // Start the requested producers
    pthread_producers = (pthread_t*)malloc(producers_count * sizeof(pthread_t));
    for(uint32_t i = 0; i < producers_count; i++) {
        REQUIRE(pthread_create(
                &pthread_producers[i],
                &attr,
                &test_epoch_gc_worker_fuzzy_separated_producer_consumer_producer_thread_func,
                (void *)&thread_data) == 0);
    }

    // Start the consumer
    REQUIRE(pthread_create(
            &epoch_gc_worker_context.pthread,
            &attr,
            &epoch_gc_worker_func,
            (void *)&epoch_gc_worker_context) == 0);

    clock_monotonic(&start_time);

    do {
        clock_monotonic(&current_time);
        sched_yield();

        clock_diff(&start_time, &current_time, &diff_time);
    } while(diff_time.tv_sec < duration);

    terminate = true;
    MEMORY_FENCE_STORE();

    // The producers will terminate before the consumer
    for(uint32_t i = 0; i < producers_count; i++) {
        pthread_join(pthread_producers[i], nullptr);
    }
    pthread_join(epoch_gc_worker_context.pthread, nullptr);

    REQUIRE(thread_data.staged_objects_counter == epoch_gc_worker_context.stats.collected_objects);

    free(pthread_producers);
}

TEST_CASE("epoch_gc_worker.c", "[epoch_gc_worker]") {
    SECTION("epoch_gc_worker_should_terminate") {
        bool terminate = false;
        epoch_gc_worker_context_t epoch_gc_worker_context = { .terminate_event_loop = &terminate };

        SECTION("should terminate") {
            terminate = true;
            REQUIRE(epoch_gc_worker_should_terminate(&epoch_gc_worker_context) == true);
        }

        SECTION("should not terminate") {
            terminate = false;
            REQUIRE(epoch_gc_worker_should_terminate(&epoch_gc_worker_context) == false);
        }
    }

    SECTION("epoch_gc_worker_log_producer_set_early_prefix_thread") {
        epoch_gc_object_type_t object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE;
        char early_prefix_cmp[16] = { 0 };
        REQUIRE(snprintf(
                early_prefix_cmp,
                sizeof(early_prefix_cmp),
                "[epoch gc %d]",
                object_type) == 12);

        epoch_gc_t *epoch_gc = epoch_gc_init(object_type);
        epoch_gc_worker_context_t epoch_gc_worker_context = { .epoch_gc = epoch_gc };

        char *early_prefix = epoch_gc_worker_log_producer_set_early_prefix_thread(
                &epoch_gc_worker_context);

        REQUIRE(strcmp(early_prefix, early_prefix_cmp) == 0);

        xalloc_free(early_prefix);
        epoch_gc_free(epoch_gc);
    }

    SECTION("epoch_gc_worker_set_thread_name") {
        epoch_gc_object_type_t object_type = EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE;
        char thread_name_current[16] = { 0 };
        char thread_name_cmp[16] = { 0 };
        REQUIRE(snprintf(
                thread_name_cmp,
                sizeof(thread_name_cmp),
                "epoch_gc_%d",
                object_type) == 10);
        epoch_gc_t *epoch_gc = epoch_gc_init(object_type);
        epoch_gc_worker_context_t epoch_gc_worker_context = { .epoch_gc = epoch_gc };

        pthread_getname_np(pthread_self(), thread_name_current, sizeof(thread_name_current));

        char *thread_name = epoch_gc_worker_set_thread_name(&epoch_gc_worker_context);

        pthread_setname_np(pthread_self(), thread_name_current);

        REQUIRE(strcmp(thread_name, thread_name_cmp) == 0);

        xalloc_free(thread_name);
        epoch_gc_free(epoch_gc);
    }

    SECTION("fuzzy staging/collecting") {
        epoch_gc_register_object_type_destructor_cb(
                EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE,
                test_epoch_gc_worker_object_destructor_cb_real);

        epoch_gc_t *epoch_gc = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_STORAGEDB_ENTRY_INDEX_LARGE);

        SECTION("one producer / one consumer") {
            test_epoch_gc_worker_fuzzy_separated_producers_consumer(
                    epoch_gc,
                    2,
                    1);
        }

        SECTION("multiple producers / one consumer") {
            test_epoch_gc_worker_fuzzy_separated_producers_consumer(
                    epoch_gc,
                    2,
                    utils_cpu_count());
        }

        epoch_gc_free(epoch_gc);
    }
}
