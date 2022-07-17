/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include <cstring>
#include <ctime>
#include <unistd.h>

#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "xalloc.h"
#include "clock.h"
#include "random.h"
#include "hugepages.h"
#include "hugepage_cache.h"
#include "thread.h"

#include "slab_allocator.h"

typedef struct mem_alloc_test_data mem_alloc_test_data_t;
struct mem_alloc_test_data {
    uint32_t ops_counter_total;
    uint32_t ops_counter_mem_alloc;
    uint64_t hash_data_x;
    uint64_t hash_data_y;
    void* memptr;
};

uint64_t test_slab_allocator_calc_hash_x(
        uint64_t x) {
    x = (x ^ (x >> 31) ^ (x >> 62)) * UINT64_C(0x319642b2d24d8ec3);
    x = (x ^ (x >> 27) ^ (x >> 54)) * UINT64_C(0x96de1b173f119089);
    x = x ^ (x >> 30) ^ (x >> 60);

    return x;
}

uint64_t test_slab_allocator_calc_hash_y(
        uint64_t y) {
    y = (y ^ (y >> 31) ^ (y >> 62)) * UINT64_C(0x3b9643b2d24d8ec3);
    y = (y ^ (y >> 27) ^ (y >> 54)) * UINT64_C(0x91de1a173f119089);
    y = y ^ (y >> 30) ^ (y >> 60);

    return y;
}

void test_slab_allocator_fuzzy_single_thread_single_size(
        uint32_t duration,
        size_t object_size,
        uint32_t min_used_slots,
        uint32_t use_max_hugepages) {
    timespec_t start_time, current_time, diff_time;
    uint32_t ops_counter_total = 0,
            ops_counter_mem_alloc = 0,
            ops_counter_mem_free = 0;

    // The object size must be large enough to hold the test data, 32 bytes, so it's not
    // possible to test the 16 bytes slab allocator
    assert(object_size >= sizeof(mem_alloc_test_data_t));

    // The calculation for the max slots to use is not 100% correct as it doesn't take into
    // account the slab header (64 bytes) and the padding placed before the actual data to
    // page-align them so it's critical to ALWAYS have at least 1 hugepage more than the
    // ones passed to this test function
    uint32_t max_used_slots =
            (use_max_hugepages * HUGEPAGE_SIZE_2MB) /
            (object_size + sizeof(slab_slot_t));

    // If there is enough space to place the signature of the object ALSO at the end set
    // this flag to true and the code will take care of copying the signature from the
    // beginning of the allocated memory to the end as well for validation
    bool can_place_signature_at_end = object_size > (sizeof(mem_alloc_test_data_t) * 2);

    double_linked_list_t *list = double_linked_list_init();

    clock_monotonic(&start_time);

    do {
        clock_monotonic(&current_time);

        ops_counter_total++;

        // Catch the overflow, no reason to perform more than 4 billion of tests
        if (ops_counter_total == 0) {
            break;
        }

        if (list->count < min_used_slots ||
            (random_generate() % 1000 > 500 && list->count < max_used_slots)) {
            // allocate memory
            ops_counter_mem_alloc++;

            void* memptr = slab_allocator_mem_alloc_zero(object_size);

            mem_alloc_test_data_t *data = (mem_alloc_test_data_t*)memptr;
            data->ops_counter_total = ops_counter_total;
            data->ops_counter_mem_alloc = ops_counter_mem_alloc;
            data->hash_data_x = test_slab_allocator_calc_hash_x(ops_counter_total);
            data->hash_data_y = test_slab_allocator_calc_hash_y(ops_counter_mem_alloc);
            data->memptr = memptr;

            if (can_place_signature_at_end) {
                memcpy(
                        (void*)((uintptr_t)memptr + object_size - sizeof(mem_alloc_test_data_t)),
                        memptr,
                        sizeof(mem_alloc_test_data_t));
            }

            double_linked_list_item_t *item = double_linked_list_item_init();
            item->data = memptr;
            double_linked_list_push_item(list, item);
        } else {
            ops_counter_mem_free++;

            double_linked_list_item_t *item = double_linked_list_shift_item(list);

            mem_alloc_test_data_t *data = (mem_alloc_test_data_t*)item->data;

            uint64_t hash_data_x = test_slab_allocator_calc_hash_x(data->ops_counter_total);
            uint64_t hash_data_y = test_slab_allocator_calc_hash_y(data->ops_counter_mem_alloc);

            REQUIRE(data->hash_data_x == hash_data_x);
            REQUIRE(data->hash_data_y == hash_data_y);
            REQUIRE(data->memptr == item->data);

            if (can_place_signature_at_end) {
                int res = memcmp(
                        (void*)((uintptr_t)data->memptr + object_size - sizeof(mem_alloc_test_data_t)),
                        data->memptr,
                        sizeof(mem_alloc_test_data_t));
                REQUIRE(res == 0);
            }

            slab_allocator_mem_free(item->data);
            double_linked_list_item_free(item);
        }

        clock_diff(&diff_time, &current_time, &start_time);
    } while(diff_time.tv_sec < duration);

    while(list->count > 0) {
        double_linked_list_item_t *item = double_linked_list_shift_item(list);
        slab_allocator_mem_free(item->data);
        double_linked_list_item_free(item);
    }

    REQUIRE(list->head == nullptr);
    REQUIRE(list->tail == nullptr);

    double_linked_list_free(list);
}

TEST_CASE("slab_allocator.c", "[slab_allocator]") {
    if (hugepages_2mb_is_available(128)) {
        hugepage_cache_init();

        SECTION("slab_allocator_init") {
            slab_allocator_t* slab_allocator = slab_allocator_init(128);

            REQUIRE(slab_allocator->object_size == 128);
            REQUIRE(slab_allocator->metrics.objects_inuse_count == 0);
            REQUIRE(slab_allocator->metrics.slices_inuse_count == 0);
            REQUIRE(slab_allocator->slots->count == 0);
            REQUIRE(slab_allocator->slices->count == 0);

            slab_allocator_free(slab_allocator);
        }

        SECTION("slab_index_by_object_size") {
            REQUIRE(slab_index_by_object_size(SLAB_OBJECT_SIZE_16 - 1) == 0);
            REQUIRE(slab_index_by_object_size(SLAB_OBJECT_SIZE_16) == 0);
            REQUIRE(slab_index_by_object_size(SLAB_OBJECT_SIZE_32) == 1);
            REQUIRE(slab_index_by_object_size(SLAB_OBJECT_SIZE_64) == 2);
            REQUIRE(slab_index_by_object_size(SLAB_OBJECT_SIZE_128) == 3);
            REQUIRE(slab_index_by_object_size(SLAB_OBJECT_SIZE_256) == 4);
            REQUIRE(slab_index_by_object_size(SLAB_OBJECT_SIZE_512) == 5);
            REQUIRE(slab_index_by_object_size(SLAB_OBJECT_SIZE_1024) == 6);
            REQUIRE(slab_index_by_object_size(SLAB_OBJECT_SIZE_2048) == 7);
            REQUIRE(slab_index_by_object_size(SLAB_OBJECT_SIZE_4096) == 8);
            REQUIRE(slab_index_by_object_size(SLAB_OBJECT_SIZE_8192) == 9);
            REQUIRE(slab_index_by_object_size(SLAB_OBJECT_SIZE_16384) == 10);
            REQUIRE(slab_index_by_object_size(SLAB_OBJECT_SIZE_32768) == 11);
            REQUIRE(slab_index_by_object_size(SLAB_OBJECT_SIZE_65536) == 12);
        }

        SECTION("sizeof(slab_slice_t)") {
            SECTION("ensure padding in slab_slice_t overlaps prev and next in double_linked_list_item") {
                slab_slice_t slice = { nullptr };
                REQUIRE(sizeof(slice.data.padding) ==
                    (sizeof(slice.double_linked_list_item.prev) + sizeof(slice.double_linked_list_item.next)));
                REQUIRE(slice.data.padding[0] == slice.double_linked_list_item.prev);
                REQUIRE(slice.data.padding[1] == slice.double_linked_list_item.next);
                REQUIRE((void*)slice.data.slab_allocator == slice.double_linked_list_item.data);
            }

            SECTION("ensure that slab_slice_t is 64 bytes to allow slab_slot_t to be cache-aligned") {
                REQUIRE(sizeof(slab_slice_t) == 64);
            }
        }

        SECTION("sizeof(slab_slot_t)") {
            SECTION("ensure padding in slab_slot_t overlaps prev and next in double_linked_list_item") {
                slab_slot_t slot = { nullptr };
                REQUIRE(sizeof(slot.data.padding) ==
                    (sizeof(slot.double_linked_list_item.prev) + sizeof(slot.double_linked_list_item.next)));
                REQUIRE(slot.data.padding[0] == slot.double_linked_list_item.prev);
                REQUIRE(slot.data.padding[1] == slot.double_linked_list_item.next);
                REQUIRE((void*)slot.data.memptr == slot.double_linked_list_item.data);
            }

            SECTION("ensure that slab_slot_t is 32 bytes to be cache-aligned") {
                REQUIRE(sizeof(slab_slot_t) == 32);
            }
        }

        SECTION("slab_allocator_slice_calculate_usable_hugepage_size") {
            REQUIRE(slab_allocator_slice_calculate_usable_hugepage_size() ==
                    HUGEPAGE_SIZE_2MB - xalloc_get_page_size() - sizeof(slab_slice_t));
        }

        SECTION("slab_allocator_slice_calculate_data_offset") {
            size_t usable_hugepage_size = 4096 * 4;
            size_t object_size = 32;
            uint32_t slots_count = (int)(usable_hugepage_size / (object_size + sizeof(slab_slot_t)));
            size_t data_offset = sizeof(slab_slice_t) + (slots_count * sizeof(slab_slot_t));
            data_offset += xalloc_get_page_size() - (data_offset % xalloc_get_page_size());

            REQUIRE(slab_allocator_slice_calculate_data_offset(
                    usable_hugepage_size,
                    object_size) == data_offset);
        }

        SECTION("slab_allocator_slice_calculate_slots_count") {
            size_t usable_hugepage_size = 4096 * 4;
            uint32_t data_offset = 2048;
            uint32_t object_size = 32;
            REQUIRE(slab_allocator_slice_calculate_slots_count(
                    usable_hugepage_size,
                    data_offset,
                    object_size) == ((usable_hugepage_size - data_offset) / object_size));
        }

        SECTION("slab_allocator_slice_init") {
            void* memptr = malloc(sizeof(slab_slice_t));
            slab_allocator_t* slab_allocator = slab_allocator_init(256);
            slab_slice_t* slab_slice = slab_allocator_slice_init(slab_allocator, memptr);

            size_t usable_hugepage_size = slab_allocator_slice_calculate_usable_hugepage_size();
            uint32_t data_offset = slab_allocator_slice_calculate_data_offset(
                    usable_hugepage_size,
                    slab_allocator->object_size);
            uint32_t slots_count = slab_allocator_slice_calculate_slots_count(
                    usable_hugepage_size,
                    data_offset,
                    slab_allocator->object_size);

            REQUIRE(slab_slice->data.slab_allocator == slab_allocator);
            REQUIRE(slab_slice->data.metrics.objects_total_count == slots_count);
            REQUIRE(slab_slice->data.metrics.objects_inuse_count == 0);
            REQUIRE(slab_slice->data.data_addr == (uintptr_t)memptr + data_offset);
            REQUIRE(slab_slice->data.available == true);

            slab_allocator_free(slab_allocator);
            free(memptr);
        }

        SECTION("slab_allocator_slice_add_slots_to_per_thread_metadata_slots") {
            size_t slab_page_size = HUGEPAGE_SIZE_2MB;
            void* memptr = malloc(slab_page_size);

            slab_allocator_t* slab_allocator = slab_allocator_init(256);
            slab_slice_t* slab_slice = slab_allocator_slice_init(slab_allocator, memptr);

            slab_allocator_slice_add_slots_to_per_thread_metadata_slots(slab_allocator, slab_slice);

            REQUIRE(slab_allocator->slots->tail == &slab_slice->data.slots[0].double_linked_list_item);
            REQUIRE(slab_allocator->slots->head ==
                    &slab_slice->data.slots[slab_slice->data.metrics.objects_total_count - 1].double_linked_list_item);

            for(int i = 0; i < slab_slice->data.metrics.objects_total_count; i++) {
                REQUIRE(slab_slice->data.slots[i].data.available == true);
            }

            slab_allocator_free(slab_allocator);
            free(memptr);
        }

        SECTION("slab_allocator_slice_remove_slots_from_per_thread_metadata_slots") {
            size_t slab_page_size = HUGEPAGE_SIZE_2MB;
            void* memptr = malloc(slab_page_size);

            slab_allocator_t* slab_allocator = slab_allocator_init(256);
            slab_slice_t* slab_slice = slab_allocator_slice_init(
                    slab_allocator,
                    memptr);

            slab_allocator_slice_add_slots_to_per_thread_metadata_slots(slab_allocator, slab_slice);
            slab_allocator_slice_remove_slots_from_per_thread_metadata_slots(slab_allocator, slab_slice);

            REQUIRE(slab_allocator->slots->tail == nullptr);
            REQUIRE(slab_allocator->slots->head == nullptr);
            REQUIRE(slab_slice->data.slots[0].data.available == true);

            for(int i = 0; i < slab_slice->data.metrics.objects_total_count; i++) {
                REQUIRE(slab_slice->data.slots[i].double_linked_list_item.next == nullptr);
                REQUIRE(slab_slice->data.slots[i].double_linked_list_item.prev == nullptr);
            }

            slab_allocator_free(slab_allocator);
            free(memptr);
        }

        SECTION("slab_allocator_grow") {
            void* hugepage_addr = hugepage_cache_pop();
            slab_slice_t* slab_slice = (slab_slice_t*)hugepage_addr;

            slab_allocator_t* slab_allocator = slab_allocator_init(256);

            slab_allocator_grow(slab_allocator, hugepage_addr);

            REQUIRE(slab_slice->data.available == false);
            REQUIRE(&slab_allocator->slices->head->data == &slab_slice->double_linked_list_item.data);
            REQUIRE(&slab_allocator->slices->tail->data == &slab_slice->double_linked_list_item.data);
            REQUIRE(slab_allocator->slots->tail == &slab_slice->data.slots[0].double_linked_list_item);
            REQUIRE(slab_allocator->slots->head ==
                    &slab_slice->data.slots[slab_slice->data.metrics.objects_total_count - 1].double_linked_list_item);

            slab_allocator_free(slab_allocator);
        }

        SECTION("slab_allocator_predefined_allocators_init / slab_allocator_predefined_allocators_free") {
            slab_allocator_enable(true);
            slab_allocator_t **slab_allocators = slab_allocator_thread_cache_init();

            for(int i = 0; i < SLAB_PREDEFINED_OBJECT_SIZES_COUNT; i++) {
                uint32_t slab_predefined_object_size = slab_predefined_object_sizes[i];
                slab_allocator_t* slab_allocator = slab_allocators[slab_index_by_object_size(
                        slab_predefined_object_size)];

                if (slab_allocator == nullptr) {
                    continue;
                }

                REQUIRE(slab_allocator->object_size == slab_predefined_object_size);
            }

            slab_allocator_thread_cache_free(slab_allocators);
            slab_allocator_enable(false);
        }

        SECTION("slab_allocator_mem_alloc_hugepages") {
            slab_allocator_enable(true);

            slab_allocator_thread_cache_set(slab_allocator_thread_cache_init());

            SECTION("allocate 1 object") {
                slab_allocator_t *slab_allocator = slab_allocator_thread_cache_get_slab_allocator_by_size(
                        slab_predefined_object_sizes[0]);

                void *memptr = slab_allocator_mem_alloc_hugepages(slab_predefined_object_sizes[0]);

                REQUIRE(slab_allocator->metrics.slices_inuse_count == 1);
                REQUIRE(slab_allocator->slots->tail->data == memptr);
                REQUIRE(((slab_slot_t *) slab_allocator->slots->tail)->data.available == false);
                REQUIRE(((slab_slot_t *) slab_allocator->slots->head)->data.available == true);
                REQUIRE(((slab_slice_t *) slab_allocator->slices->head)->data.metrics.objects_inuse_count == 1);
                REQUIRE(((slab_slot_t *) slab_allocator->slots->head)->data.available == true);
                REQUIRE(((slab_slot_t *) slab_allocator->slots->head)->data.available == true);
                REQUIRE(((slab_slot_t *) slab_allocator->slots->tail)->data.available == false);
                REQUIRE(((slab_slot_t *) slab_allocator->slots->tail)->data.memptr == memptr);
            }

            SECTION("fill one page") {
                slab_allocator_t *slab_allocator = slab_allocator_thread_cache_get_slab_allocator_by_size(
                        slab_predefined_object_sizes[0]);

                size_t usable_hugepage_size = slab_allocator_slice_calculate_usable_hugepage_size();
                uint32_t data_offset = slab_allocator_slice_calculate_data_offset(
                        usable_hugepage_size,
                        slab_allocator->object_size);
                uint32_t slots_count = slab_allocator_slice_calculate_slots_count(
                        usable_hugepage_size,
                        data_offset,
                        slab_allocator->object_size);

                for (int i = 0; i < slots_count; i++) {
                    void *memptr = slab_allocator_mem_alloc_hugepages(
                            slab_predefined_object_sizes[0]);
                }

                REQUIRE(slab_allocator->metrics.slices_inuse_count == 1);
                REQUIRE(((slab_slice_t *) slab_allocator->slices->head)->data.metrics.objects_inuse_count == slots_count);
                REQUIRE(((slab_slot_t *) slab_allocator->slots->head)->data.available == false);
                REQUIRE(((slab_slot_t *) slab_allocator->slots->head->next)->data.available == false);
                REQUIRE(((slab_slot_t *) slab_allocator->slots->tail)->data.available == false);
                REQUIRE(((slab_slot_t *) slab_allocator->slots->tail->prev)->data.available == false);
            }

            SECTION("trigger second page creation") {
                slab_allocator_t *slab_allocator = slab_allocator_thread_cache_get_slab_allocator_by_size(
                        slab_predefined_object_sizes[0]);

                size_t usable_hugepage_size = slab_allocator_slice_calculate_usable_hugepage_size();
                uint32_t data_offset = slab_allocator_slice_calculate_data_offset(
                        usable_hugepage_size,
                        slab_allocator->object_size);
                uint32_t slots_count = slab_allocator_slice_calculate_slots_count(
                        usable_hugepage_size,
                        data_offset,
                        slab_allocator->object_size);

                for (int i = 0; i < slots_count + 1; i++) {
                    void *memptr = slab_allocator_mem_alloc_hugepages(
                            slab_predefined_object_sizes[0]);
                }

                REQUIRE(slab_allocator->metrics.slices_inuse_count == 2);
                REQUIRE(slab_allocator->slices->head != slab_allocator->slices->tail);
                REQUIRE(slab_allocator->slices->head->next == slab_allocator->slices->tail);
                REQUIRE(slab_allocator->slices->head == slab_allocator->slices->tail->prev);
                REQUIRE(((slab_slice_t *) slab_allocator->slices->head)->data.metrics.objects_inuse_count == slots_count);
                REQUIRE(((slab_slice_t *) slab_allocator->slices->tail)->data.metrics.objects_inuse_count == 1);
                REQUIRE(((slab_slot_t *) slab_allocator->slots->head)->data.available == true);
                REQUIRE(((slab_slot_t *) slab_allocator->slots->head->next)->data.available == true);
                REQUIRE(((slab_slot_t *) slab_allocator->slots->tail)->data.available == false);
                REQUIRE(((slab_slot_t *) slab_allocator->slots->tail->prev)->data.available == false);
            }

            slab_allocator_thread_cache_free(slab_allocator_thread_cache_get());
            slab_allocator_thread_cache_set(nullptr);
            slab_allocator_enable(false);
        }

        SECTION("slab_allocator_mem_free_hugepages") {
            slab_allocator_enable(true);
            slab_allocator_thread_cache_set(slab_allocator_thread_cache_init());

            SECTION("allocate and free 1 object") {
                slab_allocator_t *slab_allocator = slab_allocator_thread_cache_get_slab_allocator_by_size(
                        slab_predefined_object_sizes[0]);

                void *memptr = slab_allocator_mem_alloc_hugepages(slab_predefined_object_sizes[0]);

                REQUIRE(slab_allocator->metrics.objects_inuse_count == 1);
                REQUIRE(slab_allocator->metrics.slices_inuse_count == 1);
                REQUIRE(slab_allocator->slots->head->data != memptr);
                REQUIRE(slab_allocator->slots->tail->data == memptr);

                slab_allocator_mem_free_hugepages(memptr);

                REQUIRE(slab_allocator->metrics.objects_inuse_count == 0);
                REQUIRE(slab_allocator->metrics.slices_inuse_count == 0);
                REQUIRE(slab_allocator->slots->head == nullptr);
                REQUIRE(slab_allocator->slots->tail == nullptr);
            }

            SECTION("allocate and free 1 object on different cores") {
                slab_allocator_t *slab_allocator = slab_allocator_thread_cache_get_slab_allocator_by_size(
                        slab_predefined_object_sizes[0]);

                void *memptr = slab_allocator_mem_alloc_hugepages(slab_predefined_object_sizes[0]);

                REQUIRE(slab_allocator->metrics.slices_inuse_count == 1);
                REQUIRE(slab_allocator->metrics.objects_inuse_count == 1);
                REQUIRE(slab_allocator->slots->head != nullptr);
                REQUIRE(slab_allocator->slots->tail != nullptr);
                REQUIRE(slab_allocator->slots->head->data != memptr);
                REQUIRE(slab_allocator->slots->tail->data == memptr);

                thread_current_set_affinity(1);

                slab_allocator_mem_free_hugepages(memptr);

                thread_current_set_affinity(0);

                REQUIRE(slab_allocator->metrics.objects_inuse_count == 0);
                REQUIRE(slab_allocator->metrics.slices_inuse_count == 0);
                REQUIRE(slab_allocator->slots->head == nullptr);
                REQUIRE(slab_allocator->slots->tail == nullptr);
            }

            SECTION("fill and free one hugepage") {
                slab_allocator_t *slab_allocator = slab_allocator_thread_cache_get_slab_allocator_by_size(
                        slab_predefined_object_sizes[0]);

                size_t usable_hugepage_size = slab_allocator_slice_calculate_usable_hugepage_size();
                uint32_t data_offset = slab_allocator_slice_calculate_data_offset(
                        usable_hugepage_size,
                        slab_allocator->object_size);
                uint32_t slots_count = slab_allocator_slice_calculate_slots_count(
                        usable_hugepage_size,
                        data_offset,
                        slab_allocator->object_size);

                void** memptrs = (void**)malloc(sizeof(void*) * slots_count);
                for(int i = 0; i < slots_count; i++) {
                    memptrs[i] = slab_allocator_mem_alloc_hugepages(slab_predefined_object_sizes[0]);
                }

                REQUIRE(slab_allocator->metrics.slices_inuse_count == 1);
                REQUIRE(slab_allocator->metrics.objects_inuse_count == slots_count);
                REQUIRE(slab_allocator->slots->head != nullptr);
                REQUIRE(slab_allocator->slots->tail != nullptr);

                for(int i = 0; i < slots_count; i++) {
                    slab_allocator_mem_free_hugepages(memptrs[i]);
                }

                REQUIRE(slab_allocator->metrics.objects_inuse_count == 0);
                REQUIRE(slab_allocator->metrics.slices_inuse_count == 0);
                REQUIRE(slab_allocator->slots->head == nullptr);
                REQUIRE(slab_allocator->slots->tail == nullptr);
            }

            SECTION("fill and free one hugepage and one element") {
                slab_allocator_t *slab_allocator = slab_allocator_thread_cache_get_slab_allocator_by_size(
                        slab_predefined_object_sizes[0]);

                size_t usable_hugepage_size = slab_allocator_slice_calculate_usable_hugepage_size();
                uint32_t data_offset = slab_allocator_slice_calculate_data_offset(
                        usable_hugepage_size,
                        slab_allocator->object_size);
                uint32_t slots_count = slab_allocator_slice_calculate_slots_count(
                        usable_hugepage_size,
                        data_offset,
                        slab_allocator->object_size);

                slots_count++;

                void** memptrs = (void**)malloc(sizeof(void*) * slots_count);
                for(int i = 0; i < slots_count; i++) {
                    memptrs[i] = slab_allocator_mem_alloc_hugepages(slab_predefined_object_sizes[0]);
                }

                REQUIRE(slab_allocator->metrics.slices_inuse_count == 2);
                REQUIRE(slab_allocator->metrics.objects_inuse_count == slots_count);
                REQUIRE(slab_allocator->slots->head != nullptr);
                REQUIRE(slab_allocator->slots->tail != nullptr);

                for(int i = 0; i < slots_count; i++) {
                    slab_allocator_mem_free_hugepages(
                            memptrs[i]);
                }

                REQUIRE(slab_allocator->metrics.objects_inuse_count == 0);
                REQUIRE(slab_allocator->metrics.slices_inuse_count == 0);
                REQUIRE(slab_allocator->slots->head == nullptr);
                REQUIRE(slab_allocator->slots->tail == nullptr);
            }

            slab_allocator_thread_cache_free(slab_allocator_thread_cache_get());
            slab_allocator_thread_cache_set(nullptr);
            slab_allocator_enable(false);
        }

        SECTION("slab_allocator_mem_alloc_zero") {
            slab_allocator_enable(true);
            slab_allocator_thread_cache_set(slab_allocator_thread_cache_init());

            SECTION("ensure that after allocation memory is zero-ed") {
                char fixture_test_slab_allocator_mem_alloc_zero_str[32] = { 0 };
                void *memptr = slab_allocator_mem_alloc_zero(sizeof(fixture_test_slab_allocator_mem_alloc_zero_str));
                REQUIRE(memcmp(
                        (char*)memptr,
                        fixture_test_slab_allocator_mem_alloc_zero_str,
                        sizeof(fixture_test_slab_allocator_mem_alloc_zero_str)) == 0);

                slab_allocator_mem_free(memptr);
            }

            slab_allocator_thread_cache_free(slab_allocator_thread_cache_get());
            slab_allocator_thread_cache_set(nullptr);
            slab_allocator_enable(false);
        }

        SECTION("slab_allocator alloc and free - fuzzy") {
            uint32_t min_used_slots = 2500;
            uint32_t use_max_hugepages = 125;
            uint32_t max_duration = 1;

            slab_allocator_enable(true);
            slab_allocator_thread_cache_set(slab_allocator_thread_cache_init());

            SECTION("single thread / one size - size 32") {
                test_slab_allocator_fuzzy_single_thread_single_size(
                        max_duration,
                        32,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 64") {
                test_slab_allocator_fuzzy_single_thread_single_size(
                        max_duration,
                        64,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 128") {
                test_slab_allocator_fuzzy_single_thread_single_size(
                        max_duration,
                        128,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 256") {
                test_slab_allocator_fuzzy_single_thread_single_size(
                        max_duration,
                        256,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 512") {
                test_slab_allocator_fuzzy_single_thread_single_size(
                        max_duration,
                        512,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 1024") {
                test_slab_allocator_fuzzy_single_thread_single_size(
                        max_duration,
                        1024,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 1k") {
                test_slab_allocator_fuzzy_single_thread_single_size(
                        max_duration,
                        1024,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 2k") {
                test_slab_allocator_fuzzy_single_thread_single_size(
                        max_duration,
                        2048,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 4k") {
                test_slab_allocator_fuzzy_single_thread_single_size(
                        max_duration,
                        4096,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 8k") {
                test_slab_allocator_fuzzy_single_thread_single_size(
                        max_duration,
                        8192,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 16k") {
                test_slab_allocator_fuzzy_single_thread_single_size(
                        max_duration,
                        16384,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 32k") {
                test_slab_allocator_fuzzy_single_thread_single_size(
                        max_duration,
                        32768,
                        min_used_slots,
                        use_max_hugepages);
            }

            SECTION("single thread / one size - size 64k") {
                test_slab_allocator_fuzzy_single_thread_single_size(
                        max_duration,
                        65536,
                        min_used_slots,
                        use_max_hugepages);
            }

            slab_allocator_thread_cache_free(slab_allocator_thread_cache_get());
            slab_allocator_thread_cache_set(nullptr);
            slab_allocator_enable(false);
        }

        hugepage_cache_free();
    } else {
        WARN("Can't test slab allocator, hugepages not enabled or not enough hugepages for testing, at least 128 2mb hugepages are required");
    }
}
