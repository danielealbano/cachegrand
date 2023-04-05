/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include <pthread.h>

#include "exttypes.h"
#include "clock.h"
#include "random.h"
#include "memory_fences.h"
#include "thread.h"
#include "utils_cpu.h"
#include "log/log.h"
#include "fatal.h"

#include "data_structures/queue_mpmc/queue_mpmc.h"

int test_queue_mpmc_value1 = 1234;
int test_queue_mpmc_value2 = 4321;

typedef struct test_queue_mpmc_fuzzy_test_thread_info test_queue_mpmc_fuzzy_test_thread_info_t;
struct test_queue_mpmc_fuzzy_test_thread_info {
    pthread_t thread;
    uint32_t cpu_index;
    bool_volatile_t *start;
    bool_volatile_t *stop;
    bool_volatile_t stopped;
    queue_mpmc_t *queue;
    uint32_t min_length;
    uint32_t max_length;
    uint32_volatile_t *ops_counter_total;
    uint32_volatile_t *ops_counter_push;
};

typedef struct test_queue_mpmc_fuzzy_test_data test_queue_mpmc_fuzzy_test_data_t;
struct test_queue_mpmc_fuzzy_test_data {
    uint32_t ops_counter_total;
    uint32_t ops_counter_push;
    uint64_t hash_data_x;
    uint64_t hash_data_y;
};

uint64_t test_queue_mpmc_calc_hash_x(
        uint64_t x) {
    x = (x ^ (x >> 31) ^ (x >> 62)) * UINT64_C(0x319642b2d24d8ec3);
    x = (x ^ (x >> 27) ^ (x >> 54)) * UINT64_C(0x96de1b173f119089);
    x = x ^ (x >> 30) ^ (x >> 60);

    return x;
}

uint64_t test_queue_mpmc_calc_hash_y(
        uint64_t y) {
    y = (y ^ (y >> 31) ^ (y >> 62)) * UINT64_C(0x3b9643b2d24d8ec3);
    y = (y ^ (y >> 27) ^ (y >> 54)) * UINT64_C(0x91de1a173f119089);
    y = y ^ (y >> 30) ^ (y >> 60);

    return y;
}

void *test_queue_mpmc_fuzzy_multi_thread_thread_func(
        void *user_data) {
    auto ti = (test_queue_mpmc_fuzzy_test_thread_info_t*)user_data;

    queue_mpmc *queue_mpmc = ti->queue;
    uint32_t min_length  = ti->min_length;
    uint32_t max_length = ti->max_length;

    thread_current_set_affinity(ti->cpu_index);

    do {
        MEMORY_FENCE_LOAD();
    } while(!*ti->start);

    while(!*ti->stop) {
        MEMORY_FENCE_LOAD();

        uint32_t ops_counter_total = __atomic_fetch_add(ti->ops_counter_total, 1, __ATOMIC_RELAXED);

        uint32_t queue_mpmc_length = queue_mpmc_get_length(queue_mpmc);

        if (queue_mpmc_length < min_length || (random_generate() % 1000 > 500 && queue_mpmc_length < max_length)) {
            uint32_t ops_counter_push = __atomic_fetch_add(ti->ops_counter_push, 1, __ATOMIC_RELAXED);

            auto data = (test_queue_mpmc_fuzzy_test_data_t*)malloc(sizeof(test_queue_mpmc_fuzzy_test_data_t));
            data->ops_counter_total = ops_counter_total;
            data->ops_counter_push = ops_counter_push;
            data->hash_data_x = test_queue_mpmc_calc_hash_x(ops_counter_total);
            data->hash_data_y = test_queue_mpmc_calc_hash_y(ops_counter_push);

            queue_mpmc_push(queue_mpmc, data);
        } else {
            auto data = (test_queue_mpmc_fuzzy_test_data_t*)queue_mpmc_pop(queue_mpmc);

            // There was an item at the time of the get length but not anymore
            if(!data) {
                continue;
            }

            uint64_t hash_data_x = test_queue_mpmc_calc_hash_x(data->ops_counter_total);
            uint64_t hash_data_y = test_queue_mpmc_calc_hash_y(data->ops_counter_push);

            if (data->hash_data_x != hash_data_x) {
                FATAL("test-queue-mpmc", "Incorrect hash x");
            }

            if (data->hash_data_y != hash_data_y) {
                FATAL("test-queue-mpmc", "Incorrect hash y");
            }

            free(data);
        }
    }

    ti->stopped = true;
    MEMORY_FENCE_STORE();

    return nullptr;
}

void test_queue_mpmc_fuzzy_multi_thread(
        uint32_t duration,
        uint32_t min_length,
        uint32_t max_length) {
    uint32_t ops_counter_total = 0, ops_counter_push = 0;
    timespec_t start_time, current_time, diff_time;
    queue_mpmc_t *queue_mpmc = queue_mpmc_init();
    bool start = false;
    bool stop = false;
    int n_cpus = utils_cpu_count();

    auto ti_list = (test_queue_mpmc_fuzzy_test_thread_info_t*)malloc(
            sizeof(test_queue_mpmc_fuzzy_test_thread_info_t) * n_cpus);

    for(int i = 0; i < n_cpus; i++) {
        test_queue_mpmc_fuzzy_test_thread_info_t *ti = &ti_list[i];

        ti->cpu_index = i;
        ti->start = &start;
        ti->stop = &stop;
        ti->stopped = false;
        ti->queue = queue_mpmc;
        ti->min_length = min_length;
        ti->max_length = max_length;
        ti->ops_counter_total = &ops_counter_total;
        ti->ops_counter_push = &ops_counter_push;

        if (pthread_create(
                &ti->thread,
                nullptr,
                test_queue_mpmc_fuzzy_multi_thread_thread_func,
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
        for(int i = 0; i < n_cpus && stopped; i++) {
            MEMORY_FENCE_LOAD();
            if (!ti_list[i].stopped) {
                stopped = false;
                continue;
            }
        }
    } while(!stopped);

    void *data;
    while((data = queue_mpmc_pop(queue_mpmc)) != nullptr) {
        free(data);
    }

    REQUIRE(queue_mpmc->head.data.nodes_page != nullptr);
    REQUIRE(queue_mpmc->head.data.node_index == -1);
    REQUIRE(queue_mpmc->head.data.length == 0);

    queue_mpmc_free(queue_mpmc);
    free(ti_list);
}

void test_queue_mpmc_fuzzy_single_thread(
        uint32_t duration,
        uint32_t min_length,
        uint32_t max_length) {
    timespec_t start_time, current_time, diff_time;
    uint32_t ops_counter_total = 0, ops_counter_push = 0;

    queue_mpmc_t *queue_mpmc = queue_mpmc_init();

    clock_monotonic(&start_time);

    do {
        clock_monotonic(&current_time);

        ops_counter_total++;

        uint32_t queue_mpmc_length = queue_mpmc_get_length(queue_mpmc);

        if (queue_mpmc_length < min_length || (random_generate() % 1000 > 500 && queue_mpmc_length < max_length)) {
            ops_counter_push++;

            auto data = (test_queue_mpmc_fuzzy_test_data_t*)malloc(sizeof(test_queue_mpmc_fuzzy_test_data_t));
            data->ops_counter_total = ops_counter_total;
            data->ops_counter_push = ops_counter_push;
            data->hash_data_x = test_queue_mpmc_calc_hash_x(ops_counter_total);
            data->hash_data_y = test_queue_mpmc_calc_hash_y(ops_counter_push);

            queue_mpmc_push(queue_mpmc, data);
        } else {

            auto data = (test_queue_mpmc_fuzzy_test_data_t*)queue_mpmc_pop(queue_mpmc);

            uint64_t hash_data_x = test_queue_mpmc_calc_hash_x(data->ops_counter_total);
            uint64_t hash_data_y = test_queue_mpmc_calc_hash_y(data->ops_counter_push);

            REQUIRE(data->hash_data_x == hash_data_x);
            REQUIRE(data->hash_data_y == hash_data_y);

            free(data);
        }

        clock_diff(&start_time, &current_time, &diff_time);
    } while(diff_time.tv_sec < duration);

    void *data;
    while((data = queue_mpmc_pop(queue_mpmc)) != nullptr) {
        free(data);
    }

    REQUIRE(queue_mpmc->head.data.nodes_page != nullptr);
    REQUIRE(queue_mpmc->head.data.node_index == -1);
    REQUIRE(queue_mpmc->head.data.length == 0);

    queue_mpmc_free(queue_mpmc);
}

TEST_CASE("data_structures/queue_mpmc/queue_mpmc.c", "[data_structures][queue_mpmc]") {
    SECTION("queue_mpmc_init") {
        queue_mpmc_t *queue = queue_mpmc_init();

        REQUIRE(queue != NULL);
        REQUIRE(queue->head._packed != 0);
        REQUIRE(queue->head.data.length == 0);
        REQUIRE(queue->head.data.version == 0);
        REQUIRE(queue->head.data.node_index == -1);
        REQUIRE(queue->head.data.nodes_page != NULL);

        queue_mpmc_free(queue);
    }

    SECTION("queue_mpmc_push") {
        queue_mpmc_t *queue_mpmc = queue_mpmc_init();

        SECTION("one value") {
            REQUIRE(queue_mpmc_push(queue_mpmc, &test_queue_mpmc_value1));
            REQUIRE(queue_mpmc->head.data.length == 1);
            REQUIRE(queue_mpmc->head.data.version == 1);
            REQUIRE(queue_mpmc->head.data.node_index == 0);
            REQUIRE(queue_mpmc->head.data.nodes_page != NULL);
            REQUIRE(queue_mpmc->head.data.nodes_page->nodes[0] == (uintptr_t)&test_queue_mpmc_value1);
        }

        SECTION("two values") {
            REQUIRE(queue_mpmc_push(queue_mpmc, &test_queue_mpmc_value1));
            REQUIRE(queue_mpmc_push(queue_mpmc, &test_queue_mpmc_value2));

            REQUIRE(queue_mpmc->head.data.length == 2);
            REQUIRE(queue_mpmc->head.data.version == 2);
            REQUIRE(queue_mpmc->head.data.node_index == 1);
            REQUIRE(queue_mpmc->head.data.nodes_page != NULL);
            REQUIRE(queue_mpmc->head.data.nodes_page->nodes[0] == (uintptr_t)&test_queue_mpmc_value1);
            REQUIRE(queue_mpmc->head.data.nodes_page->nodes[1] == (uintptr_t)&test_queue_mpmc_value2);
        }

        SECTION("entire page plus one") {
            auto nodes_page_initial = queue_mpmc->head.data.nodes_page;

            for(uint64_t i = 0; i < queue_mpmc->max_nodes_per_page + 1; i++) {
                REQUIRE(queue_mpmc_push(queue_mpmc, (void*)(i + 1)));
            }

            REQUIRE(queue_mpmc->head.data.length == queue_mpmc->max_nodes_per_page + 1);
            REQUIRE(queue_mpmc->head.data.version == queue_mpmc->max_nodes_per_page + 1);
            REQUIRE(queue_mpmc->head.data.node_index == 0);
            REQUIRE(queue_mpmc->head.data.nodes_page != nodes_page_initial);

            for(uint64_t i = 0; i < queue_mpmc->max_nodes_per_page; i++) {
                REQUIRE(nodes_page_initial->nodes[i] == (uintptr_t)(i + 1));
            }
            REQUIRE(queue_mpmc->head.data.nodes_page->nodes[0] == (uintptr_t)(queue_mpmc->max_nodes_per_page + 1));
        }

        queue_mpmc_free(queue_mpmc);
    }

    SECTION("queue_mpmc_pop") {
        queue_mpmc_t *queue_mpmc = queue_mpmc_init();

        SECTION("no values") {
            int *value_pop = (int*)queue_mpmc_pop(queue_mpmc);

            REQUIRE(queue_mpmc->head.data.length == 0);
            REQUIRE(queue_mpmc->head.data.version == 0);
            REQUIRE(queue_mpmc->head.data.node_index == -1);
            REQUIRE(queue_mpmc->head.data.nodes_page != NULL);
            REQUIRE(value_pop == NULL);
        }

        SECTION("one value") {
            queue_mpmc_push(queue_mpmc, &test_queue_mpmc_value1);
            int *value_pop = (int*)queue_mpmc_pop(queue_mpmc);

            REQUIRE(queue_mpmc->head.data.length == 0);
            REQUIRE(queue_mpmc->head.data.version == 2);
            REQUIRE(queue_mpmc->head.data.node_index == -1);
            REQUIRE(queue_mpmc->head.data.nodes_page != NULL);
            REQUIRE(queue_mpmc->head.data.nodes_page->nodes[0] == 0);
            REQUIRE(value_pop == &test_queue_mpmc_value1);
        }

        SECTION("two values") {
            queue_mpmc_push(queue_mpmc, &test_queue_mpmc_value1);
            queue_mpmc_push(queue_mpmc, &test_queue_mpmc_value2);
            int *value2_pop = (int*)queue_mpmc_pop(queue_mpmc);
            int *value1_pop = (int*)queue_mpmc_pop(queue_mpmc);

            REQUIRE(queue_mpmc->head.data.length == 0);
            REQUIRE(queue_mpmc->head.data.version == 4);
            REQUIRE(queue_mpmc->head.data.node_index == -1);
            REQUIRE(queue_mpmc->head.data.nodes_page != NULL);
            REQUIRE(queue_mpmc->head.data.nodes_page->nodes[0] == 0);
            REQUIRE(queue_mpmc->head.data.nodes_page->nodes[1] == 0);
            REQUIRE(value1_pop == &test_queue_mpmc_value1);
            REQUIRE(value2_pop == &test_queue_mpmc_value2);
        }

        SECTION("entire page plus one") {
            auto nodes_page_initial = queue_mpmc->head.data.nodes_page;

            for(uint64_t i = 0; i < queue_mpmc->max_nodes_per_page + 1; i++) {
                REQUIRE(queue_mpmc_push(queue_mpmc, (void*)(i + 1)));
            }

            for(int64_t i = queue_mpmc->max_nodes_per_page; i >= 0; i--) {
                REQUIRE(queue_mpmc_pop(queue_mpmc) == (void*)(i + 1));
            }

            REQUIRE(queue_mpmc->head.data.length == 0);
            REQUIRE(queue_mpmc->head.data.version == (queue_mpmc->max_nodes_per_page + 1) * 2);
            REQUIRE(queue_mpmc->head.data.node_index == -1);
            REQUIRE(queue_mpmc->head.data.nodes_page == nodes_page_initial);

            for(uint64_t i = 0; i < queue_mpmc->max_nodes_per_page; i++) {
                REQUIRE(nodes_page_initial->nodes[i] == 0);
            }
        }

        queue_mpmc_free(queue_mpmc);
    }

    SECTION("queue_mpmc_get_length") {
        queue_mpmc_t *queue_mpmc = queue_mpmc_init();

        SECTION("no values") {
            REQUIRE(queue_mpmc_get_length(queue_mpmc) == 0);
        }

        SECTION("one value") {
            queue_mpmc_push(queue_mpmc, &test_queue_mpmc_value1);
            REQUIRE(queue_mpmc_get_length(queue_mpmc) == 1);
        }

        SECTION("two values") {
            queue_mpmc_push(queue_mpmc, &test_queue_mpmc_value1);
            queue_mpmc_push(queue_mpmc, &test_queue_mpmc_value2);
            REQUIRE(queue_mpmc_get_length(queue_mpmc) == 2);
        }

        queue_mpmc_free(queue_mpmc);
    }

    SECTION("queue_mpmc_is_empty") {
        queue_mpmc_t *queue_mpmc = queue_mpmc_init();

        SECTION("no values") {
            REQUIRE(queue_mpmc_is_empty(queue_mpmc) == true);
        }

        SECTION("one value") {
            queue_mpmc_push(queue_mpmc, &test_queue_mpmc_value1);
            REQUIRE(queue_mpmc_is_empty(queue_mpmc) == false);
        }

        SECTION("two values") {
            queue_mpmc_push(queue_mpmc, &test_queue_mpmc_value1);
            queue_mpmc_push(queue_mpmc, &test_queue_mpmc_value2);
            REQUIRE(queue_mpmc_is_empty(queue_mpmc) == false);
        }

        queue_mpmc_free(queue_mpmc);
    }

    SECTION("fuzzy push/pop / single thread") {
        SECTION("single thread") {
            test_queue_mpmc_fuzzy_single_thread(1, 2 * 1024, 16 * 1024);
        }
        SECTION("multi thread") {
            test_queue_mpmc_fuzzy_multi_thread(1, 10 * 1024, 100 * 1024);
        }
    }
}
