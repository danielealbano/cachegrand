/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include "exttypes.h"
#include "memory_fences.h"
#include "clock.h"
#include "random.h"
#include "thread.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"

enum test_ring_bounded_queue_spsc_fuzzy_test_thread_info_action {
    test_ring_bounded_queue_spsc_fuzzy_test_thread_info_action_enqueue = 0,
    test_ring_bounded_queue_spsc_fuzzy_test_thread_info_action_dequeue = 1,
};
typedef enum test_ring_bounded_queue_spsc_fuzzy_test_thread_info_action test_ring_bounded_queue_spsc_fuzzy_test_thread_info_action_t;

typedef struct test_ring_bounded_queue_spsc_fuzzy_test_thread_info test_ring_bounded_queue_spsc_fuzzy_test_thread_info_t;
struct test_ring_bounded_queue_spsc_fuzzy_test_thread_info {
    pthread_t thread;
    uint32_t cpu_index;
    bool_volatile_t *start;
    bool_volatile_t *stop;
    bool_volatile_t stopped;
    ring_bounded_queue_spsc_voidptr_t *ring;
    test_ring_bounded_queue_spsc_fuzzy_test_thread_info_action_t action;
    uint32_volatile_t *ops_counter_total;
    uint32_volatile_t *ops_counter_enqueue;
    uint32_volatile_t *ops_counter_dequeue;
};

typedef struct test_ring_bounded_queue_spsc_fuzzy_test_data test_ring_bounded_queue_spsc_fuzzy_test_data_t;
struct test_ring_bounded_queue_spsc_fuzzy_test_data {
    uint64_t ops_counter_total;
    uint64_t ops_counter_enqueue;
    uint64_t hash_data_x;
    uint64_t hash_data_y;
};

uint64_t test_ring_bounded_queue_spsc_calc_hash_x(
        uint64_t x) {
    x = (x ^ (x >> 31) ^ (x >> 62)) * UINT64_C(0x319642b2d24d8ec3);
    x = (x ^ (x >> 27) ^ (x >> 54)) * UINT64_C(0x96de1b173f119089);
    x = x ^ (x >> 30) ^ (x >> 60);

    return x;
}

uint64_t test_ring_bounded_queue_spsc_calc_hash_y(
        uint64_t y) {
    y = (y ^ (y >> 31) ^ (y >> 62)) * UINT64_C(0x3b9643b2d24d8ec3);
    y = (y ^ (y >> 27) ^ (y >> 54)) * UINT64_C(0x91de1a173f119089);
    y = y ^ (y >> 30) ^ (y >> 60);

    return y;
}

void *test_ring_bounded_queue_spsc_fuzzy_multi_thread_thread_func(
        void *user_data) {
    auto *ti = (test_ring_bounded_queue_spsc_fuzzy_test_thread_info_t*)user_data;
    test_ring_bounded_queue_spsc_fuzzy_test_data_t *data;

    ring_bounded_queue_spsc_voidptr_t *ring_bounded_queue_spsc = ti->ring;

    thread_current_set_affinity(ti->cpu_index);

    do {
        MEMORY_FENCE_LOAD();
    } while(!*ti->start);

    while(!*ti->stop) {
        uint64_t ops_counter_total = __atomic_fetch_add(ti->ops_counter_total, 1, __ATOMIC_ACQ_REL);

        if (ti->action == test_ring_bounded_queue_spsc_fuzzy_test_thread_info_action_enqueue) {
            uint64_t ops_counter_enqueue = __atomic_fetch_add(ti->ops_counter_enqueue, 1, __ATOMIC_ACQ_REL);

            data = (test_ring_bounded_queue_spsc_fuzzy_test_data_t *)malloc(
                    sizeof(test_ring_bounded_queue_spsc_fuzzy_test_data_t));
            data->ops_counter_total = ops_counter_total;
            data->ops_counter_enqueue = ops_counter_enqueue;
            data->hash_data_x = test_ring_bounded_queue_spsc_calc_hash_x(data->ops_counter_total);
            data->hash_data_y = test_ring_bounded_queue_spsc_calc_hash_y(data->ops_counter_enqueue);

            if (!ring_bounded_queue_spsc_voidptr_enqueue(ring_bounded_queue_spsc, data)) {
                __atomic_fetch_sub(ti->ops_counter_enqueue, 1, __ATOMIC_ACQ_REL);
                free(data);
            }
        } else {
            data = (test_ring_bounded_queue_spsc_fuzzy_test_data_t*) ring_bounded_queue_spsc_voidptr_dequeue(ring_bounded_queue_spsc);

            // There was an item at the time of the get length but not anymore
            if(!data) {
                __atomic_fetch_sub(ti->ops_counter_total, 1, __ATOMIC_ACQ_REL);
                continue;
            }

            __atomic_fetch_add(ti->ops_counter_dequeue, 1, __ATOMIC_ACQ_REL);
            uint64_t hash_data_x = test_ring_bounded_queue_spsc_calc_hash_x(data->ops_counter_total);
            uint64_t hash_data_y = test_ring_bounded_queue_spsc_calc_hash_y(data->ops_counter_enqueue);

            if (data->hash_data_x != hash_data_x) {
                FATAL("test-ring-bounded-spsc", "Incorrect hash x");
            }

            if (data->hash_data_y != hash_data_y) {
                FATAL("test-ring-bounded-spsc", "Incorrect hash y");
            }

            free(data);
        }
    }

    ti->stopped = true;
    MEMORY_FENCE_STORE();

    return nullptr;
}

void test_ring_bounded_queue_spsc_fuzzy_multi_thread(
        uint32_t duration) {
    uint32_t ops_counter_total = 0, ops_counter_enqueue = 0, ops_counter_dequeue = 0;
    timespec_t start_time, current_time, diff_time;
    ring_bounded_queue_spsc_voidptr_t *rb = ring_bounded_queue_spsc_voidptr_init(4096);
    bool start = false;
    bool stop = false;

    auto *ti_list = (test_ring_bounded_queue_spsc_fuzzy_test_thread_info_t*)malloc(
            sizeof(test_ring_bounded_queue_spsc_fuzzy_test_thread_info_t) * 2);

    for(int i = 0; i < 2; i++) {
        test_ring_bounded_queue_spsc_fuzzy_test_thread_info_t *ti = &ti_list[i];

        ti->cpu_index = i;
        ti->start = &start;
        ti->stop = &stop;
        ti->stopped = false;
        ti->ring = rb;
        ti->action = i % 2 == 0
                ? test_ring_bounded_queue_spsc_fuzzy_test_thread_info_action_enqueue
                : test_ring_bounded_queue_spsc_fuzzy_test_thread_info_action_dequeue;
        ti->ops_counter_total = &ops_counter_total;
        ti->ops_counter_enqueue = &ops_counter_enqueue;
        ti->ops_counter_dequeue = &ops_counter_dequeue;

        if (pthread_create(
                &ti->thread,
                nullptr,
                test_ring_bounded_queue_spsc_fuzzy_multi_thread_thread_func,
                ti) != 0) {
            REQUIRE(false);
        }
    }

    start = true;
    MEMORY_FENCE_STORE();

    clock_monotonic(&start_time);

    do {
        clock_monotonic(&current_time);
        sched_yield();

        clock_diff(&start_time, &current_time, &diff_time);
    } while(diff_time.tv_sec < duration);

    stop = true;
    MEMORY_FENCE_STORE();

    bool stopped;
    do {
        stopped = true;
        sched_yield();

        // wait for all the threads to stop
        for(int i = 0; i < 2 && stopped; i++) {
            MEMORY_FENCE_LOAD();
            if (!ti_list[i].stopped) {
                stopped = false;
                continue;
            }
        }
    } while(!stopped);

    void *to_free_data;
    while((to_free_data = (void*)ring_bounded_queue_spsc_voidptr_dequeue(rb)) != nullptr) {
        ops_counter_dequeue++;
        free(to_free_data);
    }

    REQUIRE(ops_counter_enqueue == ops_counter_dequeue);

    ring_bounded_queue_spsc_voidptr_free(rb);
    free(ti_list);
}

void test_ring_bounded_queue_spsc_fuzzy_single_thread(
        uint32_t duration) {
    test_ring_bounded_queue_spsc_fuzzy_test_data_t *data;
    timespec_t start_time, current_time, diff_time;
    uint32_t ops_counter_total = 0, ops_counter_enqueue = 0, ops_counter_dequeue = 0;

    ring_bounded_queue_spsc_voidptr_t *rb = ring_bounded_queue_spsc_voidptr_init(4096);

    clock_monotonic(&start_time);

    do {
        bool enqueue_success = true;
        bool enqueue_or_dequeue = random_generate() % 1000 > 500;
        clock_monotonic(&current_time);

        ops_counter_total++;

        if (enqueue_or_dequeue) {
            ops_counter_enqueue++;

            data = (test_ring_bounded_queue_spsc_fuzzy_test_data_t*)malloc(sizeof(test_ring_bounded_queue_spsc_fuzzy_test_data_t));
            data->ops_counter_total = ops_counter_total;
            data->ops_counter_enqueue = ops_counter_enqueue;
            data->hash_data_x = test_ring_bounded_queue_spsc_calc_hash_x(data->ops_counter_total);
            data->hash_data_y = test_ring_bounded_queue_spsc_calc_hash_y(data->ops_counter_enqueue);

            enqueue_success = ring_bounded_queue_spsc_voidptr_enqueue(rb, data);
        }

        if (!enqueue_or_dequeue || !enqueue_success) {
            data = (test_ring_bounded_queue_spsc_fuzzy_test_data_t*) ring_bounded_queue_spsc_voidptr_dequeue(rb);

            if (data) {
                ops_counter_dequeue++;
                uint64_t hash_data_x = test_ring_bounded_queue_spsc_calc_hash_x(data->ops_counter_total);
                uint64_t hash_data_y = test_ring_bounded_queue_spsc_calc_hash_y(data->ops_counter_enqueue);

                if (data->hash_data_x != hash_data_x) {
                    FATAL("test-ring-bounded-spsc", "Incorrect hash x");
                }

                if (data->hash_data_y != hash_data_y) {
                    FATAL("test-ring-bounded-spsc", "Incorrect hash y");
                }

                free(data);
            } else {
                if (!enqueue_success) {
                    FATAL("test-ring-bounded-spsc", "enqueue and dequeue failed");
                }
            }
        }

        clock_diff(&start_time, &current_time, &diff_time);
    } while(diff_time.tv_sec < duration);

    void *to_free_data;
    while((to_free_data = (void*)ring_bounded_queue_spsc_voidptr_dequeue(rb)) != nullptr) {
        ops_counter_dequeue++;
        free(to_free_data);
    }

    REQUIRE(ops_counter_enqueue == ops_counter_dequeue);

    ring_bounded_queue_spsc_voidptr_free(rb);
}

TEST_CASE("data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc.c", "[data_structures][ring_bounded_queue_spsc]") {
    SECTION("ring_bounded_queue_spsc_voidptr_init") {
        ring_bounded_queue_spsc_voidptr_t* rb = ring_bounded_queue_spsc_voidptr_init(10);

        REQUIRE(rb != NULL);
        REQUIRE(rb->size == 16);
        REQUIRE(rb->head == 0);
        REQUIRE(rb->tail == 0);

        ring_bounded_queue_spsc_voidptr_free(rb);
    }

    SECTION("ring_bounded_queue_spsc_get_length") {
        ring_bounded_queue_spsc_voidptr_t* rb = ring_bounded_queue_spsc_voidptr_init(10);

        rb->tail = 5;
        REQUIRE(ring_bounded_queue_spsc_voidptr_get_length(rb) == 5);

        ring_bounded_queue_spsc_voidptr_free(rb);
    }

    SECTION("ring_bounded_queue_spsc_is_empty") {
        ring_bounded_queue_spsc_voidptr_t* rb = ring_bounded_queue_spsc_voidptr_init(10);

        SECTION("empty") {
            REQUIRE(ring_bounded_queue_spsc_voidptr_is_empty(rb) == true);
        }

        SECTION("not empty") {
            rb->tail = 5;
            REQUIRE(ring_bounded_queue_spsc_voidptr_is_empty(rb) == false);
        }

        ring_bounded_queue_spsc_voidptr_free(rb);
    }

    SECTION("ring_bounded_queue_spsc_is_full") {
        ring_bounded_queue_spsc_voidptr_t* rb = ring_bounded_queue_spsc_voidptr_init(10);

        SECTION("full") {
            rb->tail = rb->size;
            REQUIRE(ring_bounded_queue_spsc_voidptr_is_full(rb) == true);
        }

        SECTION("not full") {
            rb->tail = 0;
            REQUIRE(ring_bounded_queue_spsc_voidptr_is_full(rb) == false);
        }

        ring_bounded_queue_spsc_voidptr_free(rb);
    }

    SECTION("ring_bounded_queue_spsc_voidptr_enqueue") {
        bool res;
        ring_bounded_queue_spsc_voidptr_t* rb = ring_bounded_queue_spsc_voidptr_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * rb->size);

        SECTION("enqueue 1") {
            res = ring_bounded_queue_spsc_voidptr_enqueue(rb, random_values_from_memory[0]);

            REQUIRE(res == true);
            REQUIRE(rb->head == 0);
            REQUIRE(rb->tail == 1);
            REQUIRE(rb->items[0] == random_values_from_memory[0]);
        }

        SECTION("enqueue 2") {
            res = ring_bounded_queue_spsc_voidptr_enqueue(rb, random_values_from_memory[0]);
            REQUIRE(res == true);

            res = ring_bounded_queue_spsc_voidptr_enqueue(rb, random_values_from_memory[1]);
            REQUIRE(res == true);

            REQUIRE(rb->head == 0);
            REQUIRE(rb->tail == 2);
            REQUIRE(rb->items[1] == random_values_from_memory[1]);
        }

        SECTION("fill circular queue") {
            for(int i = 0; i < rb->size; i++) {
                res = ring_bounded_queue_spsc_voidptr_enqueue(rb, random_values_from_memory[i]);
                REQUIRE(res == true);
            }

            REQUIRE(rb->head == 0);
            REQUIRE(rb->tail == rb->size);
        }

        SECTION("overflow circular queue") {
            for(int i = 0; i < rb->size; i++) {
                res = ring_bounded_queue_spsc_voidptr_enqueue(rb, random_values_from_memory[i]);
                REQUIRE(res == true);
            }

            res = ring_bounded_queue_spsc_voidptr_enqueue(rb, random_values_from_memory[0]);
            REQUIRE(res == false);
        }

        ring_bounded_queue_spsc_voidptr_free(rb);
        free(random_values_from_memory);
    }

    SECTION("ring_bounded_queue_spsc_voidptr_enqueue_ptr") {
        void** ptr;
        ring_bounded_queue_spsc_voidptr_t* rb = ring_bounded_queue_spsc_voidptr_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * rb->size);

        SECTION("enqueue 1") {
            ptr = ring_bounded_queue_spsc_voidptr_enqueue_ptr(rb, random_values_from_memory[0]);

            REQUIRE(ptr == &rb->items[0]);
            REQUIRE(rb->head == 0);
            REQUIRE(rb->tail == 1);
            REQUIRE(rb->items[0] == random_values_from_memory[0]);
        }

        SECTION("enqueue 2") {
            ptr = ring_bounded_queue_spsc_voidptr_enqueue_ptr(rb, random_values_from_memory[0]);
            REQUIRE(ptr == &rb->items[0]);

            ptr = ring_bounded_queue_spsc_voidptr_enqueue_ptr(rb, random_values_from_memory[1]);
            REQUIRE(ptr == &rb->items[1]);

            REQUIRE(rb->head == 0);
            REQUIRE(rb->tail == 2);
            REQUIRE(rb->items[1] == random_values_from_memory[1]);
        }

        ring_bounded_queue_spsc_voidptr_free(rb);
        free(random_values_from_memory);
    }

    SECTION("ring_bounded_queue_spsc_peek") {
        ring_bounded_queue_spsc_voidptr_t* rb = ring_bounded_queue_spsc_voidptr_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * rb->size);

        SECTION("enqueue 1") {
            ring_bounded_queue_spsc_voidptr_enqueue(rb, random_values_from_memory[0]);

            REQUIRE(ring_bounded_queue_spsc_voidptr_peek(rb) == random_values_from_memory[0]);
        }

        SECTION("enqueue 2") {
            ring_bounded_queue_spsc_voidptr_enqueue(rb, random_values_from_memory[0]);
            ring_bounded_queue_spsc_voidptr_enqueue(rb, random_values_from_memory[1]);

            REQUIRE(ring_bounded_queue_spsc_voidptr_peek(rb) == random_values_from_memory[0]);
        }

        ring_bounded_queue_spsc_voidptr_free(rb);
        free(random_values_from_memory);
    }

    SECTION("ring_bounded_queue_spsc_voidptr_dequeue") {
        ring_bounded_queue_spsc_voidptr_t* rb = ring_bounded_queue_spsc_voidptr_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * rb->size);

        SECTION("dequeue 1") {
            ring_bounded_queue_spsc_voidptr_enqueue(rb, random_values_from_memory[0]);

            void* value = ring_bounded_queue_spsc_voidptr_dequeue(rb);

            REQUIRE(value == random_values_from_memory[0]);
            REQUIRE(rb->head == 1);
            REQUIRE(rb->tail == 1);
        }

        SECTION("dequeue 2") {
            ring_bounded_queue_spsc_voidptr_enqueue(rb, random_values_from_memory[0]);
            ring_bounded_queue_spsc_voidptr_enqueue(rb, random_values_from_memory[1]);

            void* value1 = ring_bounded_queue_spsc_voidptr_dequeue(rb);
            void* value2 = ring_bounded_queue_spsc_voidptr_dequeue(rb);

            REQUIRE(value1 == random_values_from_memory[0]);
            REQUIRE(value2 == random_values_from_memory[1]);
            REQUIRE(rb->head == 2);
            REQUIRE(rb->tail == 2);
        }

        SECTION("enqueue and dequeue twice 2") {
            ring_bounded_queue_spsc_voidptr_enqueue(rb, random_values_from_memory[0]);
            void* value1 = ring_bounded_queue_spsc_voidptr_dequeue(rb);

            ring_bounded_queue_spsc_voidptr_enqueue(rb, random_values_from_memory[1]);
            void* value2 = ring_bounded_queue_spsc_voidptr_dequeue(rb);

            REQUIRE(value1 == random_values_from_memory[0]);
            REQUIRE(value2 == random_values_from_memory[1]);
            REQUIRE(rb->head == 2);
            REQUIRE(rb->tail == 2);
        }

        SECTION("fill and empty circular queue") {
            for(int i = 0; i < rb->size; i++) {
                ring_bounded_queue_spsc_voidptr_enqueue(rb, random_values_from_memory[i]);
            }

            for(int i = 0; i < rb->size; i++) {
                void* value = ring_bounded_queue_spsc_voidptr_dequeue(rb);
                REQUIRE(value == random_values_from_memory[i]);
            }

            REQUIRE(rb->head == rb->size);
            REQUIRE(rb->tail == rb->size);
        }

        ring_bounded_queue_spsc_voidptr_free(rb);
        free(random_values_from_memory);
    }

    SECTION("fuzzy enqueue/dequeue") {
        SECTION("single thread") {
            test_ring_bounded_queue_spsc_fuzzy_single_thread(2);
        }

        SECTION("multi thread") {
            test_ring_bounded_queue_spsc_fuzzy_multi_thread(2);
        }
    }
}
