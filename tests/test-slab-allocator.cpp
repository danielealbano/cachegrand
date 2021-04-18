#include <catch2/catch.hpp>

#include <string.h>
#include <unistd.h>

#include "exttypes.h"
#include "spinlock.h"
#include "utils_cpu.h"
#include "utils_numa.h"
#include "xalloc.h"
#include "data_structures/double_linked_list/double_linked_list.h"

#include "slab_allocator.h"

TEST_CASE("slab_allocator.c", "[slab_allocator]") {
    SECTION("slab_allocator_init") {
        int numa_node_count = utils_numa_node_configured_count();
        int core_count = utils_cpu_count();
        slab_allocator_t* slab_allocator = slab_allocator_init(128);

        REQUIRE(slab_allocator->object_size == 128);
        REQUIRE(slab_allocator->numa_node_count == numa_node_count);
        REQUIRE(slab_allocator->core_count == core_count);
        REQUIRE(slab_allocator->metrics.total_slices_count == 0);
        REQUIRE(slab_allocator->metrics.free_slices_count == 0);

        for(int i = 0; i < slab_allocator->numa_node_count; i++) {
            REQUIRE(slab_allocator->numa_node_metadata[i].metrics.total_slices_count == 0);
            REQUIRE(slab_allocator->numa_node_metadata[i].metrics.free_slices_count == 0);
            REQUIRE(slab_allocator->numa_node_metadata[i].slices->count == 0);
        }

        for(int i = 0; i < slab_allocator->core_count; i++) {
            REQUIRE(slab_allocator->core_metadata[i].metrics.objects_inuse_count == 0);
            REQUIRE(slab_allocator->core_metadata[i].metrics.slices_total_count == 0);
            REQUIRE(slab_allocator->core_metadata[i].metrics.slices_inuse_count == 0);
            REQUIRE(slab_allocator->core_metadata[i].metrics.slices_free_count == 0);
            REQUIRE(slab_allocator->core_metadata[i].slots->count == 0);
            REQUIRE(slab_allocator->core_metadata[i].free_page_addr == NULL);
            REQUIRE(slab_allocator->core_metadata[i].free_page_addr_first_inited == false);
        }

        slab_allocator_free(slab_allocator);
    }

    SECTION("slab_index_by_object_size") {
        REQUIRE(slab_index_by_object_size(20) == 0);
        REQUIRE(slab_index_by_object_size(64) == 0);
        REQUIRE(slab_index_by_object_size(128) == 1);
        REQUIRE(slab_index_by_object_size(256) == 2);
        REQUIRE(slab_index_by_object_size(512) == 3);
        REQUIRE(slab_index_by_object_size(1024) == 4);
        REQUIRE(slab_index_by_object_size(2048) == 5);
        REQUIRE(slab_index_by_object_size(4096) == 6);
        REQUIRE(slab_index_by_object_size(8192) == 7);
        REQUIRE(slab_index_by_object_size(16384) == 8);
        REQUIRE(slab_index_by_object_size(32768) == 9);
        REQUIRE(slab_index_by_object_size(65536) == 10);
    }

    SECTION("sizeof(slab_slice_t)") {
        SECTION("ensure that slab_slice_t is 64 bytes to allow slab_slot_t to be cache-aligned") {
            REQUIRE(sizeof(slab_slice_t) == 64);
        }
    }

    SECTION("sizeof(slab_slot_t)") {
        SECTION("ensure that slab_slot_t is 32 bytes to be cache-aligned") {
            REQUIRE(sizeof(slab_slot_t) == 32);
        }
    }

    SECTION("slab_allocator_slice_init") {
        size_t slab_slot_size = sizeof(slab_slot_t);
        size_t slab_page_size = 2 * 1024 * 1024;
        size_t os_page_size = xalloc_get_page_size();
        size_t usable_page_size = slab_page_size - os_page_size - sizeof(slab_slice_t);

        void* memptr = malloc(sizeof(slab_slice_t));
        slab_allocator_t* slab_allocator = slab_allocator_init(256);

        slab_slice_t* slab_slice = slab_allocator_slice_init(slab_allocator, memptr);

        uint32_t item_size = slab_allocator->object_size + slab_slot_size;
        uint32_t slots_count = (int)(usable_page_size / item_size);

        uintptr_t data_addr = (uintptr_t)memptr + (slots_count * slab_slot_size);
        data_addr -= 1;
        data_addr += os_page_size - (data_addr % os_page_size);

        REQUIRE(slab_slice->data.slab_allocator == slab_allocator);
        REQUIRE(slab_slice->data.metrics.objects_total_count == slots_count);
        REQUIRE(slab_slice->data.metrics.objects_inuse_count == 0);
        REQUIRE(slab_slice->data.data_addr == (uintptr_t)data_addr);

        slab_allocator_free(slab_allocator);
        free(memptr);
    }

    SECTION("slab_allocator_slice_add_slots_to_per_core_metadata_slots") {
        uint32_t core_index = 0;
        size_t slab_page_size = 2 * 1024 * 1024;
        void* memptr = malloc(slab_page_size);

        slab_allocator_t* slab_allocator = slab_allocator_init(256);
        slab_slice_t* slab_slice = slab_allocator_slice_init(slab_allocator, memptr);
        slab_allocator_slice_add_slots_to_per_core_metadata_slots(slab_allocator, slab_slice, core_index);

        REQUIRE(slab_allocator->core_metadata[core_index].slots->tail == &slab_slice->data.slots[0].double_linked_list_item);
        REQUIRE(slab_allocator->core_metadata[core_index].slots->head ==
                &slab_slice->data.slots[slab_slice->data.metrics.objects_total_count - 1].double_linked_list_item);

        for(int i = 0; i < slab_slice->data.metrics.objects_total_count; i++) {
            REQUIRE(slab_slice->data.slots[i].data.available == true);
        }

        slab_allocator_free(slab_allocator);
        free(memptr);
    }

    SECTION("slab_allocator_slice_remove_slots_from_per_core_metadata_slots") {
        uint32_t core_index = 0;
        size_t slab_page_size = 2 * 1024 * 1024;
        void* memptr = malloc(slab_page_size);

        slab_allocator_t* slab_allocator = slab_allocator_init(256);
        slab_slice_t* slab_slice = slab_allocator_slice_init(slab_allocator, memptr);
        slab_allocator_slice_add_slots_to_per_core_metadata_slots(slab_allocator, slab_slice, core_index);

        slab_allocator_slice_remove_slots_from_per_core_metadata_slots(slab_allocator, slab_slice, core_index);

        REQUIRE(slab_allocator->core_metadata[core_index].free_page_addr == NULL);
        REQUIRE(slab_allocator->core_metadata[core_index].free_page_addr_first_inited == false);
        REQUIRE(slab_allocator->core_metadata[core_index].slots->tail == NULL);
        REQUIRE(slab_allocator->core_metadata[core_index].slots->head == NULL);
        REQUIRE(slab_slice->data.slots[0].data.available == true);

        for(int i = 0; i < slab_slice->data.metrics.objects_total_count; i++) {
            REQUIRE(slab_slice->data.slots[i].double_linked_list_item.next == NULL);
            REQUIRE(slab_slice->data.slots[i].double_linked_list_item.prev == NULL);
        }

        slab_allocator_free(slab_allocator);
        free(memptr);
    }

    SECTION("slab_allocator_grow") {
        uint32_t core_index = 0;
        uint32_t numa_node_index = 0;
        size_t slab_page_size = 2 * 1024 * 1024;
        void* memptr = malloc(slab_page_size);
        slab_slice_t* slab_slice = (slab_slice_t*)memptr;

        slab_allocator_t* slab_allocator = slab_allocator_init(256);
        slab_allocator_grow(slab_allocator, numa_node_index, core_index, memptr);

        REQUIRE(slab_allocator->metrics.total_slices_count == 1);
        REQUIRE(slab_allocator->metrics.free_slices_count == 0);
        REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].metrics.total_slices_count == 1);
        REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].metrics.free_slices_count == 0);
        REQUIRE(&slab_allocator->numa_node_metadata[numa_node_index].slices->head->data == &slab_slice->double_linked_list_item.data);
        REQUIRE(&slab_allocator->numa_node_metadata[numa_node_index].slices->tail->data == &slab_slice->double_linked_list_item.data);
        REQUIRE(slab_slice->data.available == false);
        REQUIRE(slab_allocator->core_metadata[core_index].free_page_addr == NULL);
        REQUIRE(slab_allocator->core_metadata[core_index].free_page_addr_first_inited == false);
        REQUIRE(slab_allocator->core_metadata[core_index].slots->tail == &slab_slice->data.slots[0].double_linked_list_item);
        REQUIRE(slab_allocator->core_metadata[core_index].slots->head ==
                &slab_slice->data.slots[slab_slice->data.metrics.objects_total_count - 1].double_linked_list_item);

        slab_allocator_free(slab_allocator);
        free(memptr);
    }

    SECTION("slab_allocator_predefined_allocators_init / slab_allocator_predefined_allocators_free") {
        int numa_node_count = utils_numa_node_configured_count();
        int core_count = utils_cpu_count();

        slab_allocator_enable(true);
        slab_allocator_predefined_allocators_init();

        for(int i = 0; i < SLAB_PREDEFINED_OBJECT_SIZES_COUNT; i++) {
            uint32_t slab_predefined_object_size = slab_predefined_object_sizes[i];
            slab_allocator_t* slab_allocator = slab_allocator_predefined_get_by_size(slab_predefined_object_size);

            if (slab_allocator == NULL) {
                continue;
            }

            REQUIRE(slab_allocator->object_size == slab_predefined_object_size);
            REQUIRE(slab_allocator->numa_node_count == numa_node_count);
            REQUIRE(slab_allocator->core_count == core_count);
            REQUIRE(slab_allocator->metrics.total_slices_count == 0);
            REQUIRE(slab_allocator->metrics.free_slices_count == 0);
        }

        slab_allocator_predefined_allocators_free();
        slab_allocator_enable(false);


        for(int i = 0; i < SLAB_PREDEFINED_OBJECT_SIZES_COUNT; i++) {
            uint32_t slab_predefined_object_size = slab_predefined_object_sizes[i];
            slab_allocator_t* slab_allocator = slab_allocator_predefined_get_by_size(slab_predefined_object_size);

            REQUIRE(slab_allocator == NULL);
        }
    }

    SECTION("slab_allocator_mem_alloc_hugepages") {
        slab_allocator_enable(true);
        slab_allocator_predefined_allocators_init();

        SECTION("allocate 1 object") {
            uint32_t numa_node_index = slab_allocator_get_current_thread_numa_node_index();
            uint32_t core_index = slab_allocator_get_current_thread_core_index();
            slab_allocator_t *slab_allocator = slab_allocator_predefined_get_by_size(64);

            void *memptr = slab_allocator_mem_alloc(64);

            REQUIRE(slab_allocator->metrics.total_slices_count == 1);
            REQUIRE(slab_allocator->metrics.free_slices_count == 0);
            REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].metrics.total_slices_count == 1);
            REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].metrics.free_slices_count == 0);
            REQUIRE(slab_allocator->core_metadata[core_index].slots->tail->data == memptr);
            REQUIRE(slab_allocator->core_metadata[core_index].free_page_addr != NULL);
            REQUIRE(slab_allocator->core_metadata[core_index].free_page_addr_first_inited == true);
            REQUIRE(((slab_slot_t*) slab_allocator->core_metadata[core_index].slots->tail)->data.available == false);
            REQUIRE(((slab_slot_t*) slab_allocator->core_metadata[core_index].slots->head)->data.available == true);
            REQUIRE(((slab_slice_t *) slab_allocator->numa_node_metadata[numa_node_index].slices->head)->data.metrics.objects_inuse_count == 1);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->head)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->head)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->tail)->data.available == false);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->tail)->data.memptr == memptr);
        }

        SECTION("fill one page") {
            uint32_t numa_node_index = slab_allocator_get_current_thread_numa_node_index();
            uint32_t core_index = slab_allocator_get_current_thread_core_index();
            slab_allocator_t *slab_allocator = slab_allocator_predefined_get_by_size(128);
            size_t slab_slot_size = sizeof(slab_slot_t);
            size_t slab_page_size = 2 * 1024 * 1024;
            size_t os_page_size = xalloc_get_page_size();
            size_t usable_page_size = slab_page_size - os_page_size - sizeof(slab_slice_t);
            uint32_t item_size = slab_allocator->object_size + slab_slot_size;
            uint32_t slots_count = (int)(usable_page_size / item_size);

            for(int i = 0; i < slots_count; i++) {
                void *memptr = slab_allocator_mem_alloc(128);
            }

            REQUIRE(slab_allocator->metrics.total_slices_count == 1);
            REQUIRE(slab_allocator->metrics.free_slices_count == 0);
            REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].metrics.total_slices_count == 1);
            REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].metrics.free_slices_count == 0);
            REQUIRE(((slab_slice_t *) slab_allocator->numa_node_metadata[numa_node_index].slices->head)->data.metrics.objects_inuse_count == slots_count);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->head)->data.available == false);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->head->next)->data.available == false);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->tail)->data.available == false);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->tail->prev)->data.available == false);
        }

        SECTION("trigger second page creation") {
            uint32_t numa_node_index = slab_allocator_get_current_thread_numa_node_index();
            uint32_t core_index = slab_allocator_get_current_thread_core_index();
            slab_allocator_t *slab_allocator = slab_allocator_predefined_get_by_size(256);
            size_t slab_slot_size = sizeof(slab_slot_t);
            size_t slab_page_size = 2 * 1024 * 1024;
            size_t os_page_size = xalloc_get_page_size();
            size_t usable_page_size = slab_page_size - os_page_size - sizeof(slab_slice_t);
            uint32_t item_size = slab_allocator->object_size + slab_slot_size;
            uint32_t slots_count = (int)(usable_page_size / item_size);

            for(int i = 0; i < slots_count + 1; i++) {
                void *memptr = slab_allocator_mem_alloc(256);
            }

            REQUIRE(slab_allocator->metrics.total_slices_count == 2);
            REQUIRE(slab_allocator->metrics.free_slices_count == 0);
            REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].metrics.total_slices_count == 2);
            REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].metrics.free_slices_count == 0);
            REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].slices->head !=
                    slab_allocator->numa_node_metadata[numa_node_index].slices->tail);
            REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].slices->head->next ==
                    slab_allocator->numa_node_metadata[numa_node_index].slices->tail);
            REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].slices->head ==
                    slab_allocator->numa_node_metadata[numa_node_index].slices->tail->prev);
            REQUIRE(((slab_slice_t *) slab_allocator->numa_node_metadata[numa_node_index].slices->head)->data.metrics.objects_inuse_count == slots_count);
            REQUIRE(((slab_slice_t *) slab_allocator->numa_node_metadata[numa_node_index].slices->tail)->data.metrics.objects_inuse_count == 1);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->head)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->head->next)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->tail)->data.available == false);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->tail->prev)->data.available == false);
        }

        slab_allocator_predefined_allocators_free();
        slab_allocator_enable(false);
    }

    SECTION("slab_allocator_mem_free") {
        slab_allocator_enable(true);
        slab_allocator_predefined_allocators_init();

        SECTION("allocate and free 1 object") {
            uint32_t numa_node_index = slab_allocator_get_current_thread_numa_node_index();
            uint32_t core_index = slab_allocator_get_current_thread_core_index();
            slab_allocator_t *slab_allocator = slab_allocator_predefined_get_by_size(512);

            void *memptr = slab_allocator_mem_alloc(512);

            REQUIRE(slab_allocator->core_metadata[core_index].slots->head->data != memptr);
            REQUIRE(slab_allocator->core_metadata[core_index].slots->tail->data == memptr);

            slab_allocator_mem_free(memptr);

            REQUIRE(slab_allocator->metrics.total_slices_count == 1);
            REQUIRE(slab_allocator->metrics.free_slices_count == 0);
            REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].metrics.total_slices_count == 1);
            REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].metrics.free_slices_count == 0);
            REQUIRE(((slab_slice_t *) slab_allocator->numa_node_metadata[numa_node_index].slices->head)->data.metrics.objects_inuse_count == 0);
            REQUIRE(((slab_slice_t *) slab_allocator->numa_node_metadata[numa_node_index].slices->head)->data.available == false);
            REQUIRE(slab_allocator->core_metadata[core_index].slots->head->data == memptr);
            REQUIRE(slab_allocator->core_metadata[core_index].slots->tail->data != memptr);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->head)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->head->next)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->tail)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->tail->prev)->data.available == true);
        }

        SECTION("fill and free one page") {
            uint32_t numa_node_index = slab_allocator_get_current_thread_numa_node_index();
            uint32_t core_index = slab_allocator_get_current_thread_core_index();
            slab_allocator_t *slab_allocator = slab_allocator_predefined_get_by_size(1024);
            size_t slab_slot_size = sizeof(slab_slot_t);
            size_t slab_page_size = 2 * 1024 * 1024;
            size_t os_page_size = xalloc_get_page_size();
            size_t usable_page_size = slab_page_size - os_page_size - sizeof(slab_slice_t);
            uint32_t item_size = slab_allocator->object_size + slab_slot_size;
            uint32_t slots_count = (int)(usable_page_size / item_size);

            void** memptrs = (void**)malloc(sizeof(void*) * slots_count);
            for(int i = 0; i < slots_count; i++) {
                memptrs[i] = slab_allocator_mem_alloc(1024);
            }

            for(int i = 0; i < slots_count; i++) {
                slab_allocator_mem_free(memptrs[i]);
            }

            REQUIRE(slab_allocator->metrics.total_slices_count == 1);
            REQUIRE(slab_allocator->metrics.free_slices_count == 0);
            REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].metrics.total_slices_count == 1);
            REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].metrics.free_slices_count == 0);
            REQUIRE(((slab_slice_t *) slab_allocator->numa_node_metadata[numa_node_index].slices->head)->data.metrics.objects_inuse_count == 0);
            REQUIRE(((slab_slice_t *) slab_allocator->numa_node_metadata[numa_node_index].slices->head)->data.available == false);
            REQUIRE(slab_allocator->core_metadata[core_index].slots->head->data == memptrs[slots_count - 1]);
            REQUIRE(slab_allocator->core_metadata[core_index].slots->tail->data == memptrs[0]);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->head)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->head->next)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->tail)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->tail->prev)->data.available == true);
        }

        SECTION("fill and free one page and one element") {
            uint32_t numa_node_index = slab_allocator_get_current_thread_numa_node_index();
            uint32_t core_index = slab_allocator_get_current_thread_core_index();
            slab_allocator_t *slab_allocator = slab_allocator_predefined_get_by_size(2048);
            size_t slab_slot_size = sizeof(slab_slot_t);
            size_t slab_page_size = 2 * 1024 * 1024;
            size_t os_page_size = xalloc_get_page_size();
            size_t usable_page_size = slab_page_size - os_page_size - sizeof(slab_slice_t);
            uint32_t item_size = slab_allocator->object_size + slab_slot_size;
            uint32_t slots_count = (int)(usable_page_size / item_size);

            slots_count++;

            void** memptrs = (void**)malloc(sizeof(void*) * slots_count);
            for(int i = 0; i < slots_count; i++) {
                memptrs[i] = slab_allocator_mem_alloc(2048);
            }

            for(int i = 0; i < slots_count; i++) {
                slab_allocator_mem_free(memptrs[i]);
            }

            REQUIRE(slab_allocator->metrics.total_slices_count == 2);
            REQUIRE(slab_allocator->metrics.free_slices_count == 1);
            REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].metrics.total_slices_count == 2);
            REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].metrics.free_slices_count == 1);
            REQUIRE(slab_allocator->numa_node_metadata[numa_node_index].slices->head !=
                    slab_allocator->numa_node_metadata[numa_node_index].slices->tail);
            REQUIRE(((slab_slice_t *) slab_allocator->numa_node_metadata[numa_node_index].slices->head)->data.metrics.objects_inuse_count == 0);
            REQUIRE(((slab_slice_t *) slab_allocator->numa_node_metadata[numa_node_index].slices->head)->data.available == true);
            REQUIRE(((slab_slice_t *) slab_allocator->numa_node_metadata[numa_node_index].slices->tail)->data.metrics.objects_inuse_count == 0);
            REQUIRE(((slab_slice_t *) slab_allocator->numa_node_metadata[numa_node_index].slices->tail)->data.available == false);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->head)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->head->next)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->tail)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->core_metadata[core_index].slots->tail->prev)->data.available == true);
        }

        slab_allocator_predefined_allocators_free();
        slab_allocator_enable(false);
    }

    SECTION("slab_allocator_mem_alloc_zero") {;
        SECTION("ensure that after reallocation memory is zero-ed") {
            const char* fixture_test_slab_allocator_mem_alloc_zero_str = "THIS IS A TEST";

            void *memptr1 = slab_allocator_mem_alloc_zero(64);

            strcpy((char*)memptr1, fixture_test_slab_allocator_mem_alloc_zero_str);

            REQUIRE(strcmp((char*)memptr1, fixture_test_slab_allocator_mem_alloc_zero_str) == 0);

            slab_allocator_mem_free(memptr1);
            void *memptr2 = slab_allocator_mem_alloc_zero(64);

            REQUIRE(memptr1 == memptr2);
            REQUIRE(strcmp((char*)memptr2, fixture_test_slab_allocator_mem_alloc_zero_str) != 0);

            slab_allocator_mem_free(memptr2);
        }
    }
}
