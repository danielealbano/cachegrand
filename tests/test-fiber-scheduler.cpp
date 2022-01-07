#include <catch2/catch.hpp>
#include <unistd.h>
#include <sys/mman.h>
#include <xalloc.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>

#include "signals_support.h"
#include "fiber.h"
#include "fiber_scheduler.h"

char test_fiber_scheduler_fixture_fiber_name[] = "test-fiber";
size_t test_fiber_scheduler_fixture_fiber_name_leb = sizeof(test_fiber_scheduler_fixture_fiber_name);
long test_fiber_scheduler_fixture_user_data = 42;

extern thread_local fiber_scheduler_stack_t fiber_scheduler_stack;

bool test_fiber_scheduler_switched_to_fiber = false;
void *test_fiber_scheduler_caller_user_data = NULL;
int8_t test_fiber_scheduler_fiber_scheduler_stack_index = -1;
int8_t test_fiber_scheduler_fiber_scheduler_stack_size = 0;
fiber_t *test_fiber_scheduler_fiber_scheduler_stack_current_fiber = NULL;


sigjmp_buf test_fiber_scheduler_jump_fp;
void test_fiber_scheduler_memory_stack_protection_signal_sigabrt_and_sigsegv_handler_longjmp(int signal_number) {
    siglongjmp(test_fiber_scheduler_jump_fp, 1);
}

void test_fiber_scheduler_memory_stack_protection_setup_sigabrt_and_sigsegv_signal_handler() {
    signals_support_register_signal_handler(
            SIGABRT,
            test_fiber_scheduler_memory_stack_protection_signal_sigabrt_and_sigsegv_handler_longjmp,
            NULL);

    signals_support_register_signal_handler(
            SIGSEGV,
            test_fiber_scheduler_memory_stack_protection_signal_sigabrt_and_sigsegv_handler_longjmp,
            NULL);
}

void test_fiber_scheduler_fiber_do_nothing(void *user_data) {
    test_fiber_scheduler_switched_to_fiber = true;
    test_fiber_scheduler_caller_user_data = user_data;
}

void test_fiber_scheduler_fiber_get_userdata_entrypoint(void *user_data) {
    test_fiber_scheduler_switched_to_fiber = true;
    test_fiber_scheduler_caller_user_data = user_data;

    fiber_scheduler_switch_back();
}

void test_fiber_scheduler_fiber_switch_to_test_entrypoint(fiber_t *from, fiber_t *to) {
    test_fiber_scheduler_switched_to_fiber = true;

    test_fiber_scheduler_fiber_scheduler_stack_index = fiber_scheduler_stack.index;
    test_fiber_scheduler_fiber_scheduler_stack_size = fiber_scheduler_stack.size;
    test_fiber_scheduler_fiber_scheduler_stack_current_fiber =
            fiber_scheduler_stack.list[test_fiber_scheduler_fiber_scheduler_stack_index];

    fiber_scheduler_switch_back();
}

TEST_CASE("fiber_scheduler.c", "[fiber_scheduler]") {
    size_t page_size = getpagesize();
    size_t stack_size = page_size * 8;
    test_fiber_scheduler_caller_user_data = NULL;
    test_fiber_scheduler_switched_to_fiber = false;

    SECTION("fiber_scheduler_get_current") {
        fiber_t fiber = {
                .start_fp = NULL,
                .start_fp_user_data = NULL,
                .name = test_fiber_scheduler_fixture_fiber_name,
                .terminate = false,
        };
        fiber_scheduler_stack.list = (fiber_t**)xalloc_alloc(sizeof(fiber_t*) * 1);
        fiber_scheduler_stack.index = 0;
        fiber_scheduler_stack.size = 1;

        fiber_scheduler_stack.list[0] = &fiber;

        REQUIRE(fiber_scheduler_get_current() == &fiber);

        xalloc_free(fiber_scheduler_stack.list);
        fiber_scheduler_stack.list = NULL;
        fiber_scheduler_stack.index = -1;
        fiber_scheduler_stack.size = 0;
    }

    SECTION("fiber_scheduler_terminate_current_fiber") {
        fiber_t fiber = {
                .start_fp = NULL,
                .start_fp_user_data = NULL,
                .name = test_fiber_scheduler_fixture_fiber_name,
                .terminate = false,
        };
        fiber_scheduler_stack.list = (fiber_t**)xalloc_alloc(sizeof(fiber_t*) * 1);
        fiber_scheduler_stack.index = 0;
        fiber_scheduler_stack.size = 1;

        fiber_scheduler_stack.list[0] = &fiber;
        fiber_scheduler_terminate_current_fiber();

        REQUIRE(fiber.terminate);

        xalloc_free(fiber_scheduler_stack.list);
        fiber_scheduler_stack.list = NULL;
        fiber_scheduler_stack.index = -1;
        fiber_scheduler_stack.size = 0;
    }

    SECTION("fiber_scheduler_stack_needs_growth") {
        SECTION("needs growth") {
            fiber_scheduler_stack.index = -1;
            fiber_scheduler_stack.size = 0;

            REQUIRE(fiber_scheduler_stack_needs_growth());
        }

        SECTION("don't need growth") {
            fiber_scheduler_stack.index = -1;
            fiber_scheduler_stack.size = 2;

            REQUIRE(!fiber_scheduler_stack_needs_growth());
        }

        fiber_scheduler_stack.list = NULL;
        fiber_scheduler_stack.index = -1;
        fiber_scheduler_stack.size = 0;
    }

    SECTION("fiber_scheduler_grow_stack") {
        SECTION("grow stack") {
            fiber_scheduler_grow_stack();

            REQUIRE(fiber_scheduler_stack.index == -1);
            REQUIRE(fiber_scheduler_stack.size == 1);
        }

        SECTION("grow stack twice") {
            fiber_scheduler_grow_stack();
            fiber_scheduler_grow_stack();

            REQUIRE(fiber_scheduler_stack.index == -1);
            REQUIRE(fiber_scheduler_stack.size == 2);
        }

        SECTION("reached max limit") {
            fiber_scheduler_stack.index = 4;
            fiber_scheduler_stack.size = 5;

            bool fatal_catched = false;
            if (sigsetjmp(test_fiber_scheduler_jump_fp, 1) == 0) {
                test_fiber_scheduler_memory_stack_protection_setup_sigabrt_and_sigsegv_signal_handler();
                fiber_scheduler_grow_stack();
            } else {
                fatal_catched = true;
            }

            // The fatal_catched variable has to be set to true as sigsetjmp on the second execution will return a value
            // different from zero.
            // A sigsegv raised by the kernel because of the memory protection means that the stack overflow protection
            // is working as intended
            REQUIRE(fatal_catched);
        }

        if (fiber_scheduler_stack.list != NULL) {
            xalloc_free(fiber_scheduler_stack.list);
            fiber_scheduler_stack.list = NULL;
        }
        fiber_scheduler_stack.index = -1;
        fiber_scheduler_stack.size = 0;
    }

    SECTION("fiber_scheduler_new_fiber_entrypoint") {
        fiber_scheduler_new_fiber_user_data_t fiber_scheduler_new_fiber_entrypoint_user_data = {
                .caller_entrypoint_fp = test_fiber_scheduler_fiber_do_nothing,
                .caller_user_data = NULL,
        };
        fiber_t fiber_1 = {
                .start_fp = NULL,
                .start_fp_user_data = NULL,
                .name = test_fiber_scheduler_fixture_fiber_name,
        };
        fiber_t *fiber_2 = fiber_new(
                test_fiber_scheduler_fixture_fiber_name,
                strlen(test_fiber_scheduler_fixture_fiber_name),
                stack_size,
                fiber_scheduler_new_fiber_entrypoint,
                &fiber_scheduler_new_fiber_entrypoint_user_data);

        fiber_scheduler_stack.list = (fiber_t**)xalloc_alloc(sizeof(fiber_t*) * 2);
        fiber_scheduler_stack.index = 1;
        fiber_scheduler_stack.size = 2;

        fiber_scheduler_stack.list[0] = &fiber_1;
        fiber_scheduler_stack.list[1] = fiber_2;

        SECTION("test entrypoint invocation") {
            fiber_context_swap(&fiber_1, fiber_2);

            REQUIRE(test_fiber_scheduler_switched_to_fiber);
            REQUIRE(fiber_2->terminate == true);
        }

        SECTION("test failure on switching to a terminated fiber") {
            fiber_context_swap(&fiber_1, fiber_2);

            // Context switching to a terminated fiber should NEVER happen as the memory will be automatically free
            // and therefore may become garbage right after the context is switched back but to test out the expected
            // behaviour of the fiber scheduler entrypoint the test does it ANYWAY.
            // The signal handler needs to handle not only the SIGABRT, as triggered by the FATAL when th terminated
            // fiber is switched back, but also the SIGSEGV as some CI/CD agents/environments (e.g. GitHub) don't really
            // like this operation.
            // Intercepting the SIGSEGV will let the test to pass even if the fiber doesn't get to invoke FATAL, can't
            // really be avoided as the memory gets freed.
            bool fatal_catched = false;
            if (sigsetjmp(test_fiber_scheduler_jump_fp, 1) == 0) {
                test_fiber_scheduler_memory_stack_protection_setup_sigabrt_and_sigsegv_signal_handler();
                fiber_context_swap(&fiber_1, fiber_2);
            } else {
                fatal_catched = true;
            }

            REQUIRE(fatal_catched);
        }

        xalloc_free(fiber_scheduler_stack.list);
        fiber_scheduler_stack.list = NULL;
        fiber_scheduler_stack.index = -1;
        fiber_scheduler_stack.size = 0;

        fiber_free(fiber_2);
    }

    SECTION("fiber_scheduler_switch_to") {
        fiber_t *fiber = fiber_new(
                test_fiber_scheduler_fixture_fiber_name,
                test_fiber_scheduler_fixture_fiber_name_leb,
                stack_size,
                test_fiber_scheduler_fiber_switch_to_test_entrypoint,
                NULL);

        fiber_scheduler_switch_to(fiber);

        REQUIRE(test_fiber_scheduler_switched_to_fiber);
        REQUIRE(fiber_scheduler_stack.list != NULL);
        REQUIRE(test_fiber_scheduler_fiber_scheduler_stack_index == 1);
        REQUIRE(test_fiber_scheduler_fiber_scheduler_stack_size == 2);
        REQUIRE(test_fiber_scheduler_fiber_scheduler_stack_current_fiber == fiber);
        REQUIRE(fiber_scheduler_stack.index == 0);
        REQUIRE(fiber_scheduler_stack.size == 2);
        REQUIRE(strcmp(fiber_scheduler_stack.list[0]->name, FIBER_SCHEDULER_FIBER_NAME) == 0);
        REQUIRE(strcmp(fiber->switched_back_on.file, CACHEGRAND_SRC_PATH) == 0);
        REQUIRE(strcmp(fiber->switched_back_on.func, "test_fiber_scheduler_fiber_switch_to_test_entrypoint") == 0);
    }

    SECTION("fiber_scheduler_new_fiber") {
        SECTION("new fiber") {
            fiber_t *fiber = fiber_scheduler_new_fiber(
                    test_fiber_scheduler_fixture_fiber_name,
                    test_fiber_scheduler_fixture_fiber_name_leb,
                    test_fiber_scheduler_fiber_get_userdata_entrypoint,
                    (void *) &test_fiber_scheduler_fixture_user_data);

            REQUIRE(strncmp(fiber->name, test_fiber_scheduler_fixture_fiber_name, strlen(test_fiber_scheduler_fixture_fiber_name)) == 0);
            REQUIRE(fiber->start_fp == fiber_scheduler_new_fiber_entrypoint);
            REQUIRE(test_fiber_scheduler_switched_to_fiber);
            REQUIRE(test_fiber_scheduler_caller_user_data == (void*)&test_fiber_scheduler_fixture_user_data);

            fiber_free(fiber);
        }
    }

    SECTION("fiber_scheduler_set_error") {
        fiber_t fiber = {
                .start_fp = NULL,
                .start_fp_user_data = NULL,
                .name = test_fiber_scheduler_fixture_fiber_name,
                .error_number = 0,
        };
        fiber_scheduler_stack.list = (fiber_t**)xalloc_alloc(sizeof(fiber_t*) * 1);
        fiber_scheduler_stack.index = 0;
        fiber_scheduler_stack.size = 1;

        fiber_scheduler_stack.list[0] = &fiber;

        fiber_scheduler_set_error(123);

        REQUIRE(fiber.error_number == 123);
        REQUIRE(errno == 123);

        xalloc_free(fiber_scheduler_stack.list);
        fiber_scheduler_stack.list = NULL;
        fiber_scheduler_stack.index = -1;
        fiber_scheduler_stack.size = 0;
    }

    SECTION("fiber_scheduler_get_error") {
        fiber_t fiber = {
                .start_fp = NULL,
                .start_fp_user_data = NULL,
                .name = test_fiber_scheduler_fixture_fiber_name,
                .error_number = 123,
        };
        fiber_scheduler_stack.list = (fiber_t**)xalloc_alloc(sizeof(fiber_t*) * 1);
        fiber_scheduler_stack.index = 0;
        fiber_scheduler_stack.size = 1;

        fiber_scheduler_stack.list[0] = &fiber;

        REQUIRE(fiber_scheduler_get_error() == 123);

        xalloc_free(fiber_scheduler_stack.list);
        fiber_scheduler_stack.list = NULL;
        fiber_scheduler_stack.index = -1;
        fiber_scheduler_stack.size = 0;
    }

    SECTION("fiber_scheduler_has_error") {
        fiber_t fiber = {
                .start_fp = NULL,
                .start_fp_user_data = NULL,
                .name = test_fiber_scheduler_fixture_fiber_name,
        };
        fiber_scheduler_stack.list = (fiber_t**)xalloc_alloc(sizeof(fiber_t*) * 1);
        fiber_scheduler_stack.index = 0;
        fiber_scheduler_stack.size = 1;

        fiber_scheduler_stack.list[0] = &fiber;

        SECTION("with error") {
            fiber.error_number = 123;
            REQUIRE(fiber_scheduler_has_error());
        }

        SECTION("without error") {
            fiber.error_number = 0;
            REQUIRE(!fiber_scheduler_has_error());
        }

        xalloc_free(fiber_scheduler_stack.list);
        fiber_scheduler_stack.list = NULL;
        fiber_scheduler_stack.index = -1;
        fiber_scheduler_stack.size = 0;
    }

    SECTION("fiber_scheduler_reset_error") {
        fiber_t fiber = {
                .start_fp = NULL,
                .start_fp_user_data = NULL,
                .name = test_fiber_scheduler_fixture_fiber_name,
                .error_number = 123,
        };
        fiber_scheduler_stack.list = (fiber_t**)xalloc_alloc(sizeof(fiber_t*) * 1);
        fiber_scheduler_stack.index = 0;
        fiber_scheduler_stack.size = 1;

        fiber_scheduler_stack.list[0] = &fiber;

        fiber_scheduler_reset_error();

        REQUIRE(fiber.error_number == 0);
        REQUIRE(errno == 0);

        xalloc_free(fiber_scheduler_stack.list);
        fiber_scheduler_stack.list = NULL;
        fiber_scheduler_stack.index = -1;
        fiber_scheduler_stack.size = 0;
    }
}
