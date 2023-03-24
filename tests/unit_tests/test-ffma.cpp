/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include <cstring>

#include "exttypes.h"
#include "spinlock.h"
#include "memory_fences.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "xalloc.h"
#include "clock.h"
#include "random.h"
#include "hugepages.h"
#include "hugepage_cache.h"
#include "thread.h"
#include "utils_cpu.h"
#include "log/log.h"
#include "fatal.h"

#include "memory_allocator/ffma.h"

typedef struct test_ffma_fuzzy_test_thread_info test_ffma_fuzzy_test_thread_info_t;
struct test_ffma_fuzzy_test_thread_info {
    pthread_t thread;
    uint32_t cpu_index;
    bool_volatile_t *start;
    bool_volatile_t *stop;
    bool_volatile_t stopped;
    uint32_t min_used_slots;
    uint32_t max_used_slots;
    uint32_t object_size;
    bool can_place_signature_at_end;
    uint32_volatile_t *ops_counter_total;
    uint32_volatile_t *ops_counter_mem_alloc;
    queue_mpmc_t *queue;
};

typedef struct test_ffma_fuzzy_test_data test_ffma_fuzzy_test_data_t;
struct test_ffma_fuzzy_test_data {
    uint32_t ops_counter_total;
    uint32_t ops_counter_mem_alloc;
    uint64_t hash_data_x;
    uint64_t hash_data_y;
    void* memptr;
};

uint64_t test_ffma_calc_hash_x(
        uint64_t x) {
    x = (x ^ (x >> 31) ^ (x >> 62)) * UINT64_C(0x319642b2d24d8ec3);
    x = (x ^ (x >> 27) ^ (x >> 54)) * UINT64_C(0x96de1b173f119089);
    x = x ^ (x >> 30) ^ (x >> 60);

    return x;
}

uint64_t test_ffma_calc_hash_y(
        uint64_t y) {
    y = (y ^ (y >> 31) ^ (y >> 62)) * UINT64_C(0x3b9643b2d24d8ec3);
    y = (y ^ (y >> 27) ^ (y >> 54)) * UINT64_C(0x91de1a173f119089);
    y = y ^ (y >> 30) ^ (y >> 60);

    return y;
}

void *test_ffma_fuzzy_multi_thread_single_size_thread_func(
        void *user_data) {
    test_ffma_fuzzy_test_thread_info_t *ti = (test_ffma_fuzzy_test_thread_info_t *)user_data;

    uint32_t min_used_slots = ti->min_used_slots;
    uint32_t max_used_slots = ti->max_used_slots;
    uint32_t object_size = ti->object_size;
    bool can_place_signature_at_end = ti->can_place_signature_at_end;
    queue_mpmc_t *queue_mpmc = ti->queue;

    thread_current_set_affinity(ti->cpu_index);

    do {
        MEMORY_FENCE_LOAD();
    } while (!*ti->start);

    while (!*ti->stop) {
        MEMORY_FENCE_LOAD();
        test_ffma_fuzzy_test_data_t *data;
        uint32_t ops_counter_total = __atomic_fetch_add(ti->ops_counter_total, 1, __ATOMIC_RELAXED);
        uint32_t queue_mpmc_length = queue_mpmc_get_length(queue_mpmc);

        if (queue_mpmc_length < min_used_slots ||
            (random_generate() % 1000 > 500 && queue_mpmc_length < max_used_slots)) {
            // allocate memory
            uint32_t ops_counter_mem_alloc = __atomic_fetch_add(ti->ops_counter_mem_alloc, 1, __ATOMIC_RELAXED);

            void* memptr = ffma_mem_alloc_zero(object_size);

            data = (test_ffma_fuzzy_test_data_t*)memptr;
            data->ops_counter_total = ops_counter_total;
            data->ops_counter_mem_alloc = ops_counter_mem_alloc;
            data->hash_data_x = test_ffma_calc_hash_x(ops_counter_total);
            data->hash_data_y = test_ffma_calc_hash_y(ops_counter_mem_alloc);
            data->memptr = memptr;

            if (can_place_signature_at_end) {
                memcpy(
                        (void*)((uintptr_t)memptr + object_size - sizeof(test_ffma_fuzzy_test_data_t)),
                        memptr,
                        sizeof(test_ffma_fuzzy_test_data_t));
            }

            queue_mpmc_push(queue_mpmc, data);
        } else {
            data = (test_ffma_fuzzy_test_data_t *)queue_mpmc_pop(queue_mpmc);

            uint64_t hash_data_x = test_ffma_calc_hash_x(data->ops_counter_total);
            uint64_t hash_data_y = test_ffma_calc_hash_y(data->ops_counter_mem_alloc);

            if (data->hash_data_x != hash_data_x) {
                // Can't use require as this code runs inside a thread, not allowed by Catch2
                FATAL("test-fast-fixed-memory-allocator", "Incorrect hash x");
            }

            if (data->hash_data_y != hash_data_y) {
                // Can't use require as this code runs inside a thread, not allowed by Catch2
                FATAL("test-fast-fixed-memory-allocator", "Incorrect hash y");
            }

            if (can_place_signature_at_end) {
                int res = memcmp(
                        (void*)((uintptr_t)data->memptr + object_size - sizeof(test_ffma_fuzzy_test_data_t)),
                        data->memptr,
                        sizeof(test_ffma_fuzzy_test_data_t));
                if (res != 0) {
                    // Can't use require as this code runs inside a thread, not allowed by Catch2
                    FATAL("test-fast-fixed-memory-allocator", "Incorrect signature");
                }
            }

            ffma_mem_free(data);
        }
    }

    ti->stopped = true;
    MEMORY_FENCE_STORE();

    return nullptr;
}

void test_ffma_fuzzy_multi_thread_single_size(
        uint32_t duration,
        uint32_t object_size,
        uint32_t min_used_slots,
        uint32_t use_max_hugepages) {
    uint32_t ops_counter_total = 0, ops_counter_mem_alloc = 0;
    timespec_t start_time, current_time, diff_time;
    queue_mpmc_t *queue_mpmc = queue_mpmc_init();
    bool start = false;
    bool stop = false;
    int n_cpus = utils_cpu_count();

    assert(object_size >= sizeof(test_ffma_fuzzy_test_data_t));

    uint32_t max_used_slots =
            (use_max_hugepages * HUGEPAGE_SIZE_2MB) /
            (object_size + sizeof(ffma_slot_t));

    bool can_place_signature_at_end = object_size > (sizeof(test_ffma_fuzzy_test_data_t) * 2);

    test_ffma_fuzzy_test_thread_info_t *ti_list =
            (test_ffma_fuzzy_test_thread_info_t*)malloc(
                    sizeof(test_ffma_fuzzy_test_thread_info_t) * n_cpus);

    for(int i = 0; i < n_cpus; i++) {
        test_ffma_fuzzy_test_thread_info_t *ti = &ti_list[i];

        ti->cpu_index = i;
        ti->start = &start;
        ti->stop = &stop;
        ti->stopped = false;
        ti->min_used_slots = min_used_slots;
        ti->max_used_slots = max_used_slots;
        ti->object_size = object_size;
        ti->can_place_signature_at_end = can_place_signature_at_end;
        ti->ops_counter_total = &ops_counter_total;
        ti->ops_counter_mem_alloc = &ops_counter_mem_alloc;
        ti->queue = queue_mpmc;

        if (pthread_create(
                &ti->thread,
                nullptr,
                test_ffma_fuzzy_multi_thread_single_size_thread_func,
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

        clock_diff(&diff_time, &current_time, &start_time);
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
        ffma_mem_free(data);
    }

    REQUIRE(queue_mpmc->head.data.node == nullptr);
    REQUIRE(queue_mpmc->head.data.length == 0);

    queue_mpmc_free(queue_mpmc);
    free(ti_list);
}

void test_ffma_fuzzy_single_thread_single_size(
        uint32_t duration,
        uint32_t object_size,
        uint32_t min_used_slots,
        uint32_t use_max_hugepages) {
    timespec_t start_time, current_time, diff_time;
    queue_mpmc_t *queue_mpmc = queue_mpmc_init();
    uint32_t ops_counter_total = 0,
            ops_counter_mem_alloc = 0;

    // The object size must be large enough to hold the test data, 32 bytes, so it's not possible to test the 16 bytes
    // object size
    assert(object_size >= sizeof(test_ffma_fuzzy_test_data_t));

    // The calculation for the max slots to use is not 100% correct as it doesn't take into account the header (64
    // bytes) and the padding placed before the actual data to page-align them so it's critical to ALWAYS have at least
    // 1 hugepage more than the ones passed to this test function
    uint32_t max_used_slots =
            (use_max_hugepages * HUGEPAGE_SIZE_2MB) /
            (object_size + sizeof(ffma_slot_t));

    // If there is enough space to place the signature of the object ALSO at the end set this flag to true and the code
    // will take care of copying the signature from the beginning of the allocated memory to the end as well for
    // validation
    bool can_place_signature_at_end = object_size > (sizeof(test_ffma_fuzzy_test_data_t) * 2);

    clock_monotonic(&start_time);

    do {
        test_ffma_fuzzy_test_data_t *data;
        clock_monotonic(&current_time);

        ops_counter_total++;
        uint32_t queue_mpmc_length = queue_mpmc_get_length(queue_mpmc);

        if (queue_mpmc_length < min_used_slots ||
            (random_generate() % 1000 > 500 && queue_mpmc_length < max_used_slots)) {
            // allocate memory
            ops_counter_mem_alloc++;

            void* memptr = ffma_mem_alloc_zero(object_size);

            data = (test_ffma_fuzzy_test_data_t*)memptr;
            data->ops_counter_total = ops_counter_total;
            data->ops_counter_mem_alloc = ops_counter_mem_alloc;
            data->hash_data_x = test_ffma_calc_hash_x(ops_counter_total);
            data->hash_data_y = test_ffma_calc_hash_y(ops_counter_mem_alloc);
            data->memptr = memptr;

            if (can_place_signature_at_end) {
                memcpy(
                        (void*)((uintptr_t)memptr + object_size - sizeof(test_ffma_fuzzy_test_data_t)),
                        memptr,
                        sizeof(test_ffma_fuzzy_test_data_t));
            }

            queue_mpmc_push(queue_mpmc, data);
        } else {
            data = (test_ffma_fuzzy_test_data_t *)queue_mpmc_pop(queue_mpmc);

            uint64_t hash_data_x = test_ffma_calc_hash_x(data->ops_counter_total);
            uint64_t hash_data_y = test_ffma_calc_hash_y(data->ops_counter_mem_alloc);

            REQUIRE(data != nullptr);
            REQUIRE(data->hash_data_x == hash_data_x);
            REQUIRE(data->hash_data_y == hash_data_y);

            if (can_place_signature_at_end) {
                int res = memcmp(
                        (void*)((uintptr_t)data->memptr + object_size - sizeof(test_ffma_fuzzy_test_data_t)),
                        data->memptr,
                        sizeof(test_ffma_fuzzy_test_data_t));
                REQUIRE(res == 0);
            }

            ffma_mem_free(data);
        }

        clock_diff(&diff_time, &current_time, &start_time);
    } while(diff_time.tv_sec < duration);

    void *data;
    while((data = queue_mpmc_pop(queue_mpmc)) != nullptr) {
        ffma_mem_free(data);
    }

    REQUIRE(queue_mpmc->head.data.node == nullptr);
    REQUIRE(queue_mpmc->head.data.length == 0);

    queue_mpmc_free(queue_mpmc);
}

TEST_CASE("ffma.c", "[ffma]") {
    if (hugepages_2mb_is_available(128)) {
        hugepage_cache_init();

        SECTION("ffma_init") {
            ffma_t* ffma = ffma_init(128);

            REQUIRE(ffma->object_size == 128);
            REQUIRE(ffma->metrics.objects_inuse_count == 0);
            REQUIRE(ffma->metrics.slices_inuse_count == 0);
            REQUIRE(ffma->slots->count == 0);
            REQUIRE(ffma->slices->count == 0);

            REQUIRE(ffma_free(ffma));
        }

        SECTION("ffma_free") {
            SECTION("without objects allocated") {
                ffma_t* ffma = ffma_init(128);

                REQUIRE(ffma->object_size == 128);
                REQUIRE(ffma->metrics.objects_inuse_count == 0);
                REQUIRE(ffma->metrics.slices_inuse_count == 0);
                REQUIRE(ffma->slots->count == 0);
                REQUIRE(ffma->slices->count == 0);

                REQUIRE(ffma_free(ffma));
            }

            SECTION("with objects allocated - locally") {
                ffma_t* ffma = ffma_init(128);

                ffma->metrics.objects_inuse_count = 1;
                REQUIRE(!ffma_free(ffma));

                ffma->metrics.objects_inuse_count = 0;
                REQUIRE(ffma_free(ffma));
            }

            SECTION("with objects allocated - in free list") {
                int value = 0;
                ffma_t* ffma = ffma_init(128);

                REQUIRE(queue_mpmc_push(ffma->free_ffma_slots_queue_from_other_threads, &value));
                REQUIRE(!ffma_free(ffma));

                REQUIRE(queue_mpmc_pop(ffma->free_ffma_slots_queue_from_other_threads) != NULL);
                REQUIRE(ffma_free(ffma));
            }
        }

        SECTION("ffma_index_by_object_size") {
            REQUIRE(ffma_index_by_object_size(FFMA_OBJECT_SIZE_16 - 1) == 0);
            REQUIRE(ffma_index_by_object_size(FFMA_OBJECT_SIZE_16) == 0);
            REQUIRE(ffma_index_by_object_size(FFMA_OBJECT_SIZE_32) == 1);
            REQUIRE(ffma_index_by_object_size(FFMA_OBJECT_SIZE_64) == 2);
            REQUIRE(ffma_index_by_object_size(FFMA_OBJECT_SIZE_128) == 3);
            REQUIRE(ffma_index_by_object_size(FFMA_OBJECT_SIZE_256) == 4);
            REQUIRE(ffma_index_by_object_size(FFMA_OBJECT_SIZE_512) == 5);
            REQUIRE(ffma_index_by_object_size(FFMA_OBJECT_SIZE_1024) == 6);
            REQUIRE(ffma_index_by_object_size(FFMA_OBJECT_SIZE_2048) == 7);
            REQUIRE(ffma_index_by_object_size(FFMA_OBJECT_SIZE_4096) == 8);
            REQUIRE(ffma_index_by_object_size(FFMA_OBJECT_SIZE_8192) == 9);
            REQUIRE(ffma_index_by_object_size(FFMA_OBJECT_SIZE_16384) == 10);
            REQUIRE(ffma_index_by_object_size(FFMA_OBJECT_SIZE_32768) == 11);
            REQUIRE(ffma_index_by_object_size(FFMA_OBJECT_SIZE_65536) == 12);
        }

        SECTION("sizeof(ffma_slice_t)") {
            SECTION("ensure padding in ffma_slice_t overlaps prev and next in double_linked_list_item") {
                ffma_slice_t slice = { nullptr };
                REQUIRE(sizeof(slice.data.padding) ==
                    (sizeof(slice.double_linked_list_item.prev) + sizeof(slice.double_linked_list_item.next)));
                REQUIRE(slice.data.padding[0] == slice.double_linked_list_item.prev);
                REQUIRE(slice.data.padding[1] == slice.double_linked_list_item.next);
                REQUIRE((void*)slice.data.ffma == slice.double_linked_list_item.data);
            }

            SECTION("ensure that ffma_slice_t is 64 bytes to allow ffma_slot_t to be cache-aligned") {
                REQUIRE(sizeof(ffma_slice_t) == 64);
            }
        }

        SECTION("sizeof(ffma_slot_t)") {
            SECTION("ensure padding in ffma_slot_t overlaps prev and next in double_linked_list_item") {
                ffma_slot_t slot = { nullptr };
                REQUIRE(sizeof(slot.data.padding) ==
                    (sizeof(slot.double_linked_list_item.prev) + sizeof(slot.double_linked_list_item.next)));
                REQUIRE(slot.data.padding[0] == slot.double_linked_list_item.prev);
                REQUIRE(slot.data.padding[1] == slot.double_linked_list_item.next);
                REQUIRE((void*)slot.data.memptr == slot.double_linked_list_item.data);
            }

            SECTION("ensure that ffma_slot_t is 32 bytes to be cache-aligned") {
                REQUIRE(sizeof(ffma_slot_t) == 32);
            }
        }

        SECTION("ffma_slice_calculate_usable_hugepage_size") {
            REQUIRE(ffma_slice_calculate_usable_hugepage_size() ==
                    HUGEPAGE_SIZE_2MB - xalloc_get_page_size() - sizeof(ffma_slice_t));
        }

        SECTION("ffma_slice_calculate_data_offset") {
            size_t usable_hugepage_size = 4096 * 4;
            uint32_t object_size = 32;
            uint32_t slots_count = (int)(usable_hugepage_size / (object_size + sizeof(ffma_slot_t)));
            size_t data_offset = sizeof(ffma_slice_t) + (slots_count * sizeof(ffma_slot_t));
            data_offset += xalloc_get_page_size() - (data_offset % xalloc_get_page_size());

            REQUIRE(ffma_slice_calculate_data_offset(
                    usable_hugepage_size,
                    object_size) == data_offset);
        }

        SECTION("ffma_slice_calculate_slots_count") {
            size_t usable_hugepage_size = 4096 * 4;
            uint32_t data_offset = 2048;
            uint32_t object_size = 32;
            REQUIRE(ffma_slice_calculate_slots_count(
                    usable_hugepage_size,
                    data_offset,
                    object_size) == ((usable_hugepage_size - data_offset) / object_size));
        }

        SECTION("ffma_slice_init") {
            void* memptr = malloc(sizeof(ffma_slice_t));
            ffma_t* ffma = ffma_init(256);
            ffma_slice_t* ffma_slice = ffma_slice_init(ffma, memptr);

            size_t usable_hugepage_size = ffma_slice_calculate_usable_hugepage_size();
            uint32_t data_offset = ffma_slice_calculate_data_offset(
                    usable_hugepage_size,
                    ffma->object_size);
            uint32_t slots_count = ffma_slice_calculate_slots_count(
                    usable_hugepage_size,
                    data_offset,
                    ffma->object_size);

            REQUIRE(ffma_slice->data.ffma == ffma);
            REQUIRE(ffma_slice->data.metrics.objects_total_count == slots_count);
            REQUIRE(ffma_slice->data.metrics.objects_inuse_count == 0);
            REQUIRE(ffma_slice->data.data_addr == (uintptr_t)memptr + data_offset);
            REQUIRE(ffma_slice->data.available == true);

            ffma_free(ffma);
            free(memptr);
        }

        SECTION("ffma_slice_add_slots_to_per_thread_metadata_slots") {
            size_t slice_size = HUGEPAGE_SIZE_2MB;
            void* memptr = malloc(slice_size);

            ffma_t* ffma = ffma_init(256);
            ffma_slice_t* ffma_slice =
                    ffma_slice_init(ffma, memptr);

            ffma_slice_add_slots_to_per_thread_metadata_slots(
                    ffma,
                    ffma_slice);

            REQUIRE(ffma->slots->tail ==
                    &ffma_slice->data.slots[0].double_linked_list_item);
            REQUIRE(ffma->slots->head ==
                    &ffma_slice->data.slots[
                            ffma_slice->data.metrics.objects_total_count - 1].double_linked_list_item);

            for(int i = 0; i < ffma_slice->data.metrics.objects_total_count; i++) {
                REQUIRE(ffma_slice->data.slots[i].data.available == true);
            }

            ffma_free(ffma);
            free(memptr);
        }

        SECTION("ffma_slice_remove_slots_from_per_thread_metadata_slots") {
            size_t slice_size = HUGEPAGE_SIZE_2MB;
            void* memptr = malloc(slice_size);

            ffma_t* ffma = ffma_init(256);
            ffma_slice_t* ffma_slice =
                    ffma_slice_init(
                        ffma,
                        memptr);

            ffma_slice_add_slots_to_per_thread_metadata_slots(ffma, ffma_slice);
            ffma_slice_remove_slots_from_per_thread_metadata_slots(ffma, ffma_slice);

            REQUIRE(ffma->slots->tail == nullptr);
            REQUIRE(ffma->slots->head == nullptr);
            REQUIRE(ffma_slice->data.slots[0].data.available == true);

            for(int i = 0; i < ffma_slice->data.metrics.objects_total_count; i++) {
                REQUIRE(ffma_slice->data.slots[i].double_linked_list_item.next == nullptr);
                REQUIRE(ffma_slice->data.slots[i].double_linked_list_item.prev == nullptr);
            }

            ffma_free(ffma);
            free(memptr);
        }

        SECTION("ffma_grow") {
            void* hugepage_addr = hugepage_cache_pop();
            ffma_slice_t* ffma_slice = (ffma_slice_t*)hugepage_addr;

            ffma_t* ffma = ffma_init(256);

            ffma_grow(ffma, hugepage_addr);

            REQUIRE(ffma_slice->data.available == false);
            REQUIRE(&ffma->slices->head->data == &ffma_slice->double_linked_list_item.data);
            REQUIRE(&ffma->slices->tail->data == &ffma_slice->double_linked_list_item.data);
            REQUIRE(ffma->slots->tail == &ffma_slice->data.slots[0].double_linked_list_item);
            REQUIRE(ffma->slots->head ==
                    &ffma_slice->data.slots[ffma_slice->data.metrics.objects_total_count - 1].double_linked_list_item);

            ffma_free(ffma);
        }

        SECTION("ffma_predefined_allocators_init / ffma_predefined_allocators_free") {
            ffma_enable(true);
            ffma_t **ffmas = ffma_thread_cache_init();

            for(int i = 0; i < FFMA_PREDEFINED_OBJECT_SIZES_COUNT; i++) {
                uint32_t ffma_predefined_object_size =
                        ffma_predefined_object_sizes[i];
                ffma_t* ffma =
                        ffmas[ffma_index_by_object_size(
                            ffma_predefined_object_size)];

                if (ffma == nullptr) {
                    continue;
                }

                REQUIRE(ffma->object_size == ffma_predefined_object_size);
            }

            ffma_thread_cache_free(ffmas);
            ffma_enable(false);
        }

        SECTION("ffma_mem_alloc_hugepages") {
            ffma_enable(true);
            ffma_t *ffma = ffma_init(ffma_predefined_object_sizes[0]);

            SECTION("allocate 1 object") {
                void *memptr = ffma_mem_alloc_hugepages(ffma, ffma_predefined_object_sizes[0]);

                REQUIRE(ffma->metrics.slices_inuse_count == 1);
                REQUIRE(queue_mpmc_get_length(ffma->free_ffma_slots_queue_from_other_threads) == 0);
                REQUIRE(ffma->slots->tail->data == memptr);
                REQUIRE(((ffma_slot_t *) ffma->slots->tail)->data.available == false);
                REQUIRE(((ffma_slot_t *) ffma->slots->head)->data.available == true);
                REQUIRE(((ffma_slice_t *) ffma->slices->head)->data.metrics.objects_inuse_count == 1);
                REQUIRE(((ffma_slot_t *) ffma->slots->head)->data.available == true);
                REQUIRE(((ffma_slot_t *) ffma->slots->head)->data.available == true);
                REQUIRE(((ffma_slot_t *) ffma->slots->tail)->data.available == false);
                REQUIRE(((ffma_slot_t *) ffma->slots->tail)->data.memptr == memptr);
            }

            SECTION("fill one page") {
                size_t usable_hugepage_size = ffma_slice_calculate_usable_hugepage_size();
                uint32_t data_offset = ffma_slice_calculate_data_offset(
                        usable_hugepage_size,
                        ffma->object_size);
                uint32_t slots_count = ffma_slice_calculate_slots_count(
                        usable_hugepage_size,
                        data_offset,
                        ffma->object_size);

                for (int i = 0; i < slots_count; i++) {
                    void *memptr = ffma_mem_alloc_hugepages(ffma, ffma_predefined_object_sizes[0]);
                }

                REQUIRE(ffma->metrics.slices_inuse_count == 1);
                REQUIRE(queue_mpmc_get_length(ffma->free_ffma_slots_queue_from_other_threads) == 0);
                REQUIRE(((ffma_slice_t *) ffma->slices->head)->data.metrics.objects_inuse_count == slots_count);
                REQUIRE(((ffma_slot_t *) ffma->slots->head)->data.available == false);
                REQUIRE(((ffma_slot_t *) ffma->slots->head->next)->data.available == false);
                REQUIRE(((ffma_slot_t *) ffma->slots->tail)->data.available == false);
                REQUIRE(((ffma_slot_t *) ffma->slots->tail->prev)->data.available == false);
            }

            SECTION("trigger second page creation") {
                size_t usable_hugepage_size = ffma_slice_calculate_usable_hugepage_size();
                uint32_t data_offset = ffma_slice_calculate_data_offset(
                        usable_hugepage_size,
                        ffma->object_size);
                uint32_t slots_count = ffma_slice_calculate_slots_count(
                        usable_hugepage_size,
                        data_offset,
                        ffma->object_size);

                for (int i = 0; i < slots_count + 1; i++) {
                    void *memptr = ffma_mem_alloc_hugepages(ffma, ffma_predefined_object_sizes[0]);
                }

                REQUIRE(ffma->metrics.slices_inuse_count == 2);
                REQUIRE(queue_mpmc_get_length(ffma->free_ffma_slots_queue_from_other_threads) == 0);
                REQUIRE(ffma->slices->head != ffma->slices->tail);
                REQUIRE(ffma->slices->head->next == ffma->slices->tail);
                REQUIRE(ffma->slices->head == ffma->slices->tail->prev);
                REQUIRE(((ffma_slice_t *) ffma->slices->head)->data.metrics.objects_inuse_count == slots_count);
                REQUIRE(((ffma_slice_t *) ffma->slices->tail)->data.metrics.objects_inuse_count == 1);
                REQUIRE(((ffma_slot_t *) ffma->slots->head)->data.available == true);
                REQUIRE(((ffma_slot_t *) ffma->slots->head->next)->data.available == true);
                REQUIRE(((ffma_slot_t *) ffma->slots->tail)->data.available == false);
                REQUIRE(((ffma_slot_t *) ffma->slots->tail->prev)->data.available == false);
            }

            ffma_free(ffma);
            ffma_enable(false);
        }

        SECTION("ffma_mem_free_hugepages") {
            ffma_t *ffma = ffma_init(ffma_predefined_object_sizes[0]);
            ffma_t *ffmas[] = { ffma };

            ffma_enable(true);
            ffma_thread_cache_set(ffmas);

            SECTION("allocate and free 1 object") {
                void *memptr = ffma_mem_alloc_hugepages(ffma, ffma_predefined_object_sizes[0]);

                REQUIRE(ffma->metrics.objects_inuse_count == 1);
                REQUIRE(ffma->metrics.slices_inuse_count == 1);
                REQUIRE(queue_mpmc_get_length(ffma->free_ffma_slots_queue_from_other_threads) == 0);
                REQUIRE(ffma->slots->head->data != memptr);
                REQUIRE(ffma->slots->tail->data == memptr);

                ffma_mem_free_hugepages(memptr);

                REQUIRE(ffma->metrics.objects_inuse_count == 0);
                REQUIRE(ffma->metrics.slices_inuse_count == 0);
                REQUIRE(queue_mpmc_get_length(ffma->free_ffma_slots_queue_from_other_threads) == 0);
                REQUIRE(ffma->slots->head == nullptr);
                REQUIRE(ffma->slots->tail == nullptr);
            }

            SECTION("allocate and free 1 object via different threads") {
                void *memptr = ffma_mem_alloc_hugepages(ffma, ffma_predefined_object_sizes[0]);

                REQUIRE(ffma->metrics.objects_inuse_count == 1);
                REQUIRE(ffma->metrics.slices_inuse_count == 1);
                REQUIRE(queue_mpmc_get_length(ffma->free_ffma_slots_queue_from_other_threads) == 0);
                REQUIRE(ffma->slots->head->data != memptr);
                REQUIRE(ffma->slots->tail->data == memptr);

                ffma_mem_free_hugepages(memptr);

                REQUIRE(ffma->metrics.objects_inuse_count == 0);
                REQUIRE(ffma->metrics.slices_inuse_count == 0);
                REQUIRE(queue_mpmc_get_length(ffma->free_ffma_slots_queue_from_other_threads) == 0);
                REQUIRE(ffma->slots->head == nullptr);
                REQUIRE(ffma->slots->tail == nullptr);
            }

            SECTION("fill and free one hugepage") {
                size_t usable_hugepage_size = ffma_slice_calculate_usable_hugepage_size();
                uint32_t data_offset = ffma_slice_calculate_data_offset(
                        usable_hugepage_size,
                        ffma->object_size);
                uint32_t slots_count = ffma_slice_calculate_slots_count(
                        usable_hugepage_size,
                        data_offset,
                        ffma->object_size);

                void** memptrs = (void**)malloc(sizeof(void*) * slots_count);
                for(int i = 0; i < slots_count; i++) {
                    memptrs[i] = ffma_mem_alloc_hugepages(ffma, ffma_predefined_object_sizes[0]);
                }

                REQUIRE(ffma->metrics.slices_inuse_count == 1);
                REQUIRE(ffma->metrics.objects_inuse_count == slots_count);
                REQUIRE(queue_mpmc_get_length(ffma->free_ffma_slots_queue_from_other_threads) == 0);
                REQUIRE(ffma->slots->head != nullptr);
                REQUIRE(ffma->slots->tail != nullptr);

                for(int i = 0; i < slots_count; i++) {
                    ffma_mem_free_hugepages(memptrs[i]);
                }

                REQUIRE(ffma->metrics.objects_inuse_count == 0);
                REQUIRE(ffma->metrics.slices_inuse_count == 0);
                REQUIRE(queue_mpmc_get_length(ffma->free_ffma_slots_queue_from_other_threads) == 0);
                REQUIRE(ffma->slots->head == nullptr);
                REQUIRE(ffma->slots->tail == nullptr);
            }

            SECTION("fill and free one hugepage and one element") {
                size_t usable_hugepage_size = ffma_slice_calculate_usable_hugepage_size();
                uint32_t data_offset = ffma_slice_calculate_data_offset(
                        usable_hugepage_size,
                        ffma->object_size);
                uint32_t slots_count = ffma_slice_calculate_slots_count(
                        usable_hugepage_size,
                        data_offset,
                        ffma->object_size);

                slots_count++;

                void** memptrs = (void**)malloc(sizeof(void*) * slots_count);
                for(int i = 0; i < slots_count; i++) {
                    memptrs[i] = ffma_mem_alloc_hugepages(ffma, ffma_predefined_object_sizes[0]);
                }

                REQUIRE(ffma->metrics.slices_inuse_count == 2);
                REQUIRE(ffma->metrics.objects_inuse_count == slots_count);
                REQUIRE(queue_mpmc_get_length(ffma->free_ffma_slots_queue_from_other_threads) == 0);
                REQUIRE(ffma->slots->head != nullptr);
                REQUIRE(ffma->slots->tail != nullptr);

                for(int i = 0; i < slots_count; i++) {
                    ffma_mem_free_hugepages(
                            memptrs[i]);
                }

                REQUIRE(ffma->metrics.objects_inuse_count == 0);
                REQUIRE(ffma->metrics.slices_inuse_count == 0);
                REQUIRE(queue_mpmc_get_length(ffma->free_ffma_slots_queue_from_other_threads) == 0);
                REQUIRE(ffma->slots->head == nullptr);
                REQUIRE(ffma->slots->tail == nullptr);
            }

            SECTION("free via different ffma") {
                ffma_t *ffma2 = ffma_init(ffma_predefined_object_sizes[0]);

                void *memptr1 = ffma_mem_alloc_hugepages(ffma2, ffma_predefined_object_sizes[0]);
                REQUIRE(ffma2->metrics.objects_inuse_count == 1);
                REQUIRE(ffma2->metrics.slices_inuse_count == 1);

                ffma_mem_free_hugepages(memptr1);

                REQUIRE(ffma2->metrics.objects_inuse_count == 1);
                REQUIRE(ffma->metrics.objects_inuse_count == 0);
                REQUIRE(ffma2->metrics.slices_inuse_count == 1);
                REQUIRE(ffma->metrics.slices_inuse_count == 0);

                REQUIRE(queue_mpmc_get_length(ffma2->free_ffma_slots_queue_from_other_threads) == 1);

                // slots from the queue are used if all the items in the hugepages have been used so it's necessary to
                // fill the hugepage allocated
                size_t usable_hugepage_size = ffma_slice_calculate_usable_hugepage_size();
                uint32_t data_offset = ffma_slice_calculate_data_offset(
                        usable_hugepage_size,
                        ffma->object_size);
                uint32_t slots_count = ffma_slice_calculate_slots_count(
                        usable_hugepage_size,
                        data_offset,
                        ffma->object_size);

                void** memptrs = (void**)malloc(sizeof(void*) * (slots_count - 1));
                for(int i = 0; i < slots_count - 1; i++) {
                    memptrs[i] = ffma_mem_alloc_hugepages(ffma2, ffma_predefined_object_sizes[0]);
                }

                // All the previous allocation must have come out from the local list of slots
                REQUIRE(queue_mpmc_get_length(ffma2->free_ffma_slots_queue_from_other_threads) == 1);

                // This last allocation must come from the free list
                void *memptr2 = ffma_mem_alloc_hugepages(ffma2, ffma_predefined_object_sizes[0]);

                REQUIRE(queue_mpmc_get_length(ffma2->free_ffma_slots_queue_from_other_threads) == 0);
                REQUIRE(ffma2->metrics.objects_inuse_count == slots_count);
                REQUIRE(ffma2->metrics.slices_inuse_count == 1);
                REQUIRE(memptr1 == memptr2);

                // Free up everything
                for(int i = 0; i < slots_count - 1; i++) {
                    ffma_mem_free_hugepages(memptrs[i]);
                }
                ffma_mem_free_hugepages(memptr2);

                REQUIRE(queue_mpmc_get_length(ffma2->free_ffma_slots_queue_from_other_threads) == slots_count);

                REQUIRE(ffma_free(ffma2));
            }

            ffma_free(ffma);
            ffma_thread_cache_set(nullptr);
            ffma_enable(false);
        }

        SECTION("ffma_mem_alloc_zero") {
            ffma_enable(true);
            ffma_thread_cache_set(ffma_thread_cache_init());

            SECTION("ensure that after allocation memory is zero-ed") {
                char fixture_test_ffma_mem_alloc_zero_str[32] = { 0 };
                void *memptr = ffma_mem_alloc_zero(sizeof(fixture_test_ffma_mem_alloc_zero_str));
                REQUIRE(memcmp(
                        (char*)memptr,
                        fixture_test_ffma_mem_alloc_zero_str,
                        sizeof(fixture_test_ffma_mem_alloc_zero_str)) == 0);

                ffma_mem_free(memptr);
            }

            ffma_thread_cache_free(ffma_thread_cache_get());
            ffma_thread_cache_set(nullptr);
            ffma_enable(false);
        }

        SECTION("ffma alloc and free - fuzzy - single thread") {
            uint32_t min_used_slots = 2500;
            uint32_t use_max_hugepages = 100;
            uint32_t max_duration = 1;

            ffma_enable(true);
            ffma_thread_cache_set(ffma_thread_cache_init());

            SECTION("single thread / one size - size 32") {
                test_ffma_fuzzy_single_thread_single_size(
                        max_duration,
                        32,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 64") {
                test_ffma_fuzzy_single_thread_single_size(
                        max_duration,
                        64,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 128") {
                test_ffma_fuzzy_single_thread_single_size(
                        max_duration,
                        128,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 256") {
                test_ffma_fuzzy_single_thread_single_size(
                        max_duration,
                        256,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 512") {
                test_ffma_fuzzy_single_thread_single_size(
                        max_duration,
                        512,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 1k") {
                test_ffma_fuzzy_single_thread_single_size(
                        max_duration,
                        1024,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 2k") {
                test_ffma_fuzzy_single_thread_single_size(
                        max_duration,
                        2048,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 4k") {
                test_ffma_fuzzy_single_thread_single_size(
                        max_duration,
                        4096,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 8k") {
                test_ffma_fuzzy_single_thread_single_size(
                        max_duration,
                        8192,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 16k") {
                test_ffma_fuzzy_single_thread_single_size(
                        max_duration,
                        16384,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 32k") {
                test_ffma_fuzzy_single_thread_single_size(
                        max_duration,
                        32768,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 64k") {
                test_ffma_fuzzy_single_thread_single_size(
                        max_duration,
                        65536,
                        min_used_slots,
                        use_max_hugepages);
            }

            ffma_thread_cache_free(ffma_thread_cache_get());
            ffma_thread_cache_set(nullptr);
            ffma_enable(false);
        }

        SECTION("ffma alloc and free - fuzzy - multi thread") {
            uint32_t min_used_slots = 2500;
            uint32_t use_max_hugepages = 100;
            uint32_t max_duration = 1;

            ffma_enable(true);
            ffma_thread_cache_set(ffma_thread_cache_init());

            SECTION("multi thread / one size - size 32") {
                test_ffma_fuzzy_multi_thread_single_size(
                        max_duration,
                        32,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("multi thread / one size - size 64") {
                test_ffma_fuzzy_multi_thread_single_size(
                        max_duration,
                        64,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("multi thread / one size - size 128") {
                test_ffma_fuzzy_multi_thread_single_size(
                        max_duration,
                        128,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("multi thread / one size - size 256") {
                test_ffma_fuzzy_multi_thread_single_size(
                        max_duration,
                        256,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("multi thread / one size - size 512") {
                test_ffma_fuzzy_multi_thread_single_size(
                        max_duration,
                        512,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("multi thread / one size - size 1024") {
                test_ffma_fuzzy_multi_thread_single_size(
                        max_duration,
                        1024,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("multi thread / one size - size 2k") {
                test_ffma_fuzzy_multi_thread_single_size(
                        max_duration,
                        2048,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("multi thread / one size - size 4k") {
                test_ffma_fuzzy_multi_thread_single_size(
                        max_duration,
                        4096,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("multi thread / one size - size 8k") {
                test_ffma_fuzzy_multi_thread_single_size(
                        max_duration,
                        8192,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("multi thread / one size - size 16k") {
                test_ffma_fuzzy_multi_thread_single_size(
                        max_duration,
                        16384,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("multi thread / one size - size 32k") {
                test_ffma_fuzzy_multi_thread_single_size(
                        max_duration,
                        32768,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("multi thread / one size - size 64k") {
                test_ffma_fuzzy_multi_thread_single_size(
                        max_duration,
                        65536,
                        min_used_slots,
                        use_max_hugepages);
            }

            ffma_thread_cache_free(ffma_thread_cache_get());
            ffma_thread_cache_set(nullptr);
            ffma_enable(false);
        }

        hugepage_cache_free();
    } else {
        WARN("Can't test fast fixed memory allocator, hugepages not enabled or not enough hugepages for testing, at least 128 2mb hugepages are required");
    }
}
