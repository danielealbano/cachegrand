/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <csignal>
#include <csetjmp>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "signals_support.h"
#include "fiber/fiber.h"

struct test_fiber_context_swap_update_user_data_and_swap_back_data {
    uint64_t *user_data_to;
    fiber_t *fiber_from;
    fiber_t *fiber_to;
};
typedef struct test_fiber_context_swap_update_user_data_and_swap_back_data
        test_fiber_context_swap_update_user_data_and_swap_back_data_t;

char test_fiber_name[] = "test-fiber";
size_t test_fiber_name_len = sizeof(test_fiber_name);

sigjmp_buf test_fiber_jump_fp;
void test_fiber_memory_stack_protection_signal_sigsegv_handler_longjmp(int signal_number) {
    siglongjmp(test_fiber_jump_fp, 1);
}

void test_fiber_memory_stack_protection_setup_sigsegv_signal_handler() {
    signals_support_register_signal_handler(
            SIGSEGV,
            test_fiber_memory_stack_protection_signal_sigsegv_handler_longjmp,
            nullptr);
}

void test_fiber_new_empty(void *user_data) {
    // do nothing
}

void test_fiber_context_swap_update_user_data_and_swap_back(void *user_data) {
    auto user_data_ = (test_fiber_context_swap_update_user_data_and_swap_back_data_t*)user_data;
    *user_data_->user_data_to = 2;

    fiber_context_swap(&user_data_->fiber_to->stack_pointer, &user_data_->fiber_from->stack_pointer);
}

int test_fiber_stack_protection_find_memory_protection(void *start_address, void* end_address) {
    FILE *fp;
    int prot_return = -1;
    char line[1024], start_address_str[17] = { 0 }, end_address_str[17] = { 0 };

    sprintf(start_address_str, "%lx", (uintptr_t)start_address);
    sprintf(end_address_str, "%lx", (uintptr_t)end_address);

    fp = fopen("/proc/self/maps", "r");
    if (fp == nullptr) {
        fprintf(stderr, "Failed to open /proc/self/maps\n");
        return 01;
    }

    while (fgets(line, sizeof(line), fp) != nullptr) {
        int cmp1 = strncmp(line, start_address_str, strlen(start_address_str));
        int cmp2 = strncmp(line + strlen(start_address_str) + 1, end_address_str, strlen(end_address_str));

        if (cmp1 != 0 || cmp2 != 0) {
            continue;
        }

        size_t prot_offset = strlen(start_address_str) + 1 + strlen(end_address_str) + 1;
        prot_return = 0;
        if (line[prot_offset + 0] == 'r') {
            prot_return |= PROT_READ;
        }
        if (line[prot_offset + 1] == 'w') {
            prot_return |= PROT_WRITE;
        }
        if (line[prot_offset + 2] == 'x') {
            prot_return |= PROT_EXEC;
        }
    }

    fclose(fp);

    return prot_return;
}

TEST_CASE("fiber.c", "[fiber]") {
    size_t page_size = getpagesize();
    size_t stack_size = 2000000;
    int internal_stderr = dup(STDERR_FILENO);
    int internal_nullfd = open("/dev/null", O_RDWR);

    SECTION("fiber_stack_protection") {
        int protection_flags;
        fiber_t fiber = { nullptr };
        fiber.stack_base = aligned_alloc(page_size, page_size * FIBER_GUARD_PAGES_COUNT);

        SECTION("test enabling the memory protection") {
            // Enable stack fiber protection
            fiber_stack_protection(&fiber, true);

            // Retrieve the protection flags of the memory page
            protection_flags = test_fiber_stack_protection_find_memory_protection(
                    fiber.stack_base,
                    (char*)fiber.stack_base + page_size * FIBER_GUARD_PAGES_COUNT);

            REQUIRE(protection_flags == PROT_NONE);

            mprotect(fiber.stack_base, page_size * FIBER_GUARD_PAGES_COUNT, PROT_READ | PROT_WRITE);
        }

        SECTION("test disabling the memory protection") {
            // Enable and disable the stack protection
            fiber_stack_protection(&fiber, true);
            fiber_stack_protection(&fiber, false);

            // Check if the page is listed in /proc/self/maps
            protection_flags = test_fiber_stack_protection_find_memory_protection(
                    fiber.stack_base, (char*)fiber.stack_base + page_size);

            // The protection flags for the allocated should be entirely missing as fiber_stack_protection resets them
            // to PROT_READ | PROT_WRITE that are the default ones.
            // This behaviour may change in the future as it's depends on the OS.
            REQUIRE(protection_flags == -1);
        }

        free(fiber.stack_base);
    }

    SECTION("fiber_new") {
        SECTION("allocate a new fiber") {
            int user_data = 0;

            fiber_t *fiber = fiber_new(
                    test_fiber_name,
                    test_fiber_name_len,
                    stack_size,
                    test_fiber_new_empty,
                    &user_data);

            REQUIRE(fiber);
            REQUIRE(fiber->start_fp == test_fiber_new_empty);
            REQUIRE(fiber->start_fp_user_data == &user_data);

            fiber_free(fiber);
        }

        SECTION("Allocate fiber without entry point") {
            fiber_t *fiber = fiber_new(
                    test_fiber_name,
                    test_fiber_name_len,
                    stack_size,
                    nullptr,
                    nullptr);

            REQUIRE(fiber);
            REQUIRE(fiber->start_fp == nullptr);
            REQUIRE(fiber->start_fp_user_data == nullptr);

            fiber_free(fiber);
        }

        SECTION("fail to allocate a new fiber without entry point but with user_data") {
            fiber_t *fiber = fiber_new(
                    test_fiber_name,
                    test_fiber_name_len,
                    0,
                    nullptr,
                    (void*)1234);
            REQUIRE(fiber == nullptr);
        }

        SECTION("fail to allocate a new fiber without stack") {
            fiber_t *fiber = fiber_new(
                    test_fiber_name,
                    test_fiber_name_len,
                    0,
                    test_fiber_new_empty,
                    nullptr);
            REQUIRE(fiber == nullptr);
        }
    }

    SECTION("fiber_free") {
        // There is no REQUIRE for this test, fiber_free is run to ensure it's not triggering a failure (if mprotect
        // fails the system will throw an error for trying to access a memory protected area)
        SECTION("allocate a new fiber") {
            fiber_t *fiber = fiber_new(
                    test_fiber_name,
                    test_fiber_name_len,
                    stack_size,
                    test_fiber_new_empty,
                    nullptr);
            fiber_free(fiber);
        }
    }

    SECTION("fiber_context_swap") {
        SECTION("test fiber context swap") {
            uint64_t user_data_to = 0;

            test_fiber_context_swap_update_user_data_and_swap_back_data_t user_data = {
                    &user_data_to,
                    nullptr,
                    nullptr
            };

            fiber_t *fiber_from = fiber_new(
                    test_fiber_name,
                    test_fiber_name_len,
                    stack_size,
                    nullptr,
                    nullptr);

            fiber_t *fiber_to = fiber_new(
                    test_fiber_name,
                    test_fiber_name_len,
                    stack_size,
                    test_fiber_context_swap_update_user_data_and_swap_back,
                    &user_data);

            user_data.fiber_from = fiber_from;
            user_data.fiber_to = fiber_to;

            fiber_context_swap(&fiber_from->stack_pointer, &fiber_to->stack_pointer);

            REQUIRE(user_data_to == 2);

            fiber_free(fiber_from);
            fiber_free(fiber_to);
        }
    }

    SECTION("test stack protection") {
        fiber_t *fiber = fiber_new(test_fiber_name, test_fiber_name_len, stack_size, test_fiber_new_empty, nullptr);

        SECTION("alter non protected memory") {
            *(uint64_t*)fiber->stack_pointer = 0;
            *(uint64_t*)((uintptr_t)fiber->stack_base + (page_size * FIBER_GUARD_PAGES_COUNT)) = 0;
        }

        // NOTE: will trigger a sigsegv, it's expected
        SECTION("alter protected memory") {
            bool fatal_caught;

            dup2(internal_nullfd, STDERR_FILENO);

            fatal_caught = false;
            if (sigsetjmp(test_fiber_jump_fp, 1) == 0) {
                test_fiber_memory_stack_protection_setup_sigsegv_signal_handler();
                *(uint64_t*)fiber->stack_base = 0;
            } else {
                fatal_caught = true;
            }

            dup2(internal_stderr, STDERR_FILENO);

            REQUIRE(fatal_caught);

            dup2(internal_nullfd, STDERR_FILENO);

            fatal_caught = false;
            if (sigsetjmp(test_fiber_jump_fp, 1) == 0) {
                test_fiber_memory_stack_protection_setup_sigsegv_signal_handler();
                *(uint64_t*)((uintptr_t)fiber->stack_base + (page_size * FIBER_GUARD_PAGES_COUNT) - 1) = 0;
            } else {
                fatal_caught = true;
            }

            dup2(internal_stderr, STDERR_FILENO);

            REQUIRE(fatal_caught);
        }

        fiber_free(fiber);
    }

    close(internal_stderr);
    close(internal_nullfd);
}
