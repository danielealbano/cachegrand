#include <catch2/catch.hpp>

#include <string.h>
#include <unistd.h>

#include "exttypes.h"
#include "spinlock.h"
#include "utils_cpu.h"
#include "xalloc.h"
#include "data_structures/double_linked_list/double_linked_list.h"

#include "slab_allocator.h"

TEST_CASE("slab_allocator.c", "[slab_allocator]") {
    SECTION("slab_allocator_init") {
        uint32_t core_count = utils_cpu_count();
        uint32_t numa_node_count = 1;

        slab_allocator_t* slab_allocator = slab_allocator_init(256);

        REQUIRE(slab_allocator->object_size == 256);
        REQUIRE(slab_allocator->slices_count == 0);

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
    }

    SECTION("sizeof(slab_slice_t)") {
        REQUIRE(sizeof(slab_slice_t) == 32);
    }

    SECTION("sizeof(slab_slot_t)") {
        REQUIRE(sizeof(slab_slot_t) == 32);
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

        REQUIRE(slab_slice->slab_allocator == slab_allocator);
        REQUIRE(slab_slice->count == slots_count);
        REQUIRE(slab_slice->data_addr == (uintptr_t)data_addr);

        slab_allocator_free(slab_allocator);
        free(memptr);
    }

    SECTION("slab_allocator_slice_add_slots_to_slots_per_core") {
        uint32_t core_index = 0;
        size_t slab_page_size = 2 * 1024 * 1024;
        void* memptr = malloc(slab_page_size);

        slab_allocator_t* slab_allocator = slab_allocator_init(256);
        slab_slice_t* slab_slice = slab_allocator_slice_init(slab_allocator, memptr);
        slab_allocator_slice_add_slots_to_slots_per_core(slab_allocator, slab_slice, core_index);

        REQUIRE(slab_allocator->slots_per_core[core_index]->tail == &slab_slice->slots[0].item);
        REQUIRE(slab_allocator->slots_per_core[core_index]->head == &slab_slice->slots[slab_slice->count - 1].item);

        for(int i = 0; i < slab_slice->count; i++) {
            REQUIRE(slab_slice->slots[i].data.available == true);
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

        REQUIRE(slab_allocator->slices_count == 1);
        REQUIRE(slab_allocator->slices_per_numa[numa_node_index]->head->data == memptr);
        REQUIRE(slab_allocator->slices_per_numa[numa_node_index]->tail->data == memptr);
        REQUIRE(slab_allocator->slots_per_core[core_index]->tail == &slab_slice->slots[0].item);
        REQUIRE(slab_allocator->slots_per_core[core_index]->head == &slab_slice->slots[slab_slice->count - 1].item);

        slab_allocator_free(slab_allocator);
        free(memptr);
    }

    SECTION("slab_allocator_mem_alloc") {
        SECTION("allocate 1 object") {
            uint32_t core_index = slab_allocator_get_current_thread_core_index();
            slab_allocator_t *slab_allocator = slab_allocator_predefined_get_by_size(64);

            void *memptr = slab_allocator_mem_alloc(64);

            REQUIRE(slab_allocator->slices_count == 1);
            REQUIRE(slab_allocator->slots_per_core[core_index]->tail->data == memptr);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->head)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->tail)->data.available == false);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->tail)->data.memptr == memptr);
        }

        SECTION("fill one page") {
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

            REQUIRE(slab_allocator->slices_count == 1);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->head)->data.available == false);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->head->next)->data.available == false);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->tail)->data.available == false);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->tail->prev)->data.available == false);
        }

        SECTION("trigger second page creation") {
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

            REQUIRE(slab_allocator->slices_count == 2);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->head)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->head->next)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->tail)->data.available == false);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->tail->prev)->data.available == false);
        }
    }

    SECTION("slab_allocator_mem_free") {
        SECTION("allocate and free 1 object") {
            uint32_t core_index = slab_allocator_get_current_thread_core_index();
            slab_allocator_t *slab_allocator = slab_allocator_predefined_get_by_size(512);

            void *memptr = slab_allocator_mem_alloc(512);

            REQUIRE(slab_allocator->slots_per_core[core_index]->tail->data == memptr);

            slab_allocator_mem_free(memptr);

            REQUIRE(slab_allocator->slices_count == 1);
            REQUIRE(slab_allocator->slots_per_core[core_index]->head->data == memptr);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->head)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->head)->data.memptr == memptr);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->tail)->data.available == true);
        }

        SECTION("fill and free one page") {
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

            REQUIRE(slab_allocator->slices_count == 1);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->head)->data.memptr == memptrs[slots_count - 1]);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->head)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->head->next)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->tail)->data.memptr == memptrs[0]);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->tail)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->tail->prev)->data.available == true);
        }

        SECTION("fill and free one page and one element") {
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

            REQUIRE(slab_allocator->slices_count == 2);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->head)->data.memptr == memptrs[slots_count - 1]);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->head)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->head->next)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->tail)->data.available == true);
            REQUIRE(((slab_slot_t *) slab_allocator->slots_per_core[core_index]->tail->prev)->data.available == true);
        }
    }
}
