/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "xalloc.h"
#include "log/log.h"
#include "fatal.h"

#include "fiber.h"

#define TAG "fiber"

static thread_local fiber_t *fiber_new_first_run_fiber = NULL;
static thread_local fiber_start_fp_t *fiber_new_first_run_fiber_start_fp = NULL;
static thread_local void *fiber_new_first_run_func_user_data = NULL;
static thread_local void **fiber_new_first_run_from = NULL, **fiber_new_first_run_to = NULL;

__attribute__((noreturn))
static void fiber_abort() {
    abort();
}

__attribute__((noreturn))
static void fiber_new_first_run() {
    fiber_start_fp_t *fiber_start_fp = fiber_new_first_run_fiber_start_fp;
    volatile void *user_data = fiber_new_first_run_func_user_data;
    volatile fiber_t *fiber = fiber_new_first_run_fiber;

    fiber_context_swap(fiber_new_first_run_to, fiber_new_first_run_from);

#if __GCC_HAVE_DWARF2_CFI_ASM && __amd64
    asm (".cfi_undefined rip");
#endif

    fiber_start_fp((void *)user_data);

    FATAL(
            TAG,
            "Critical error, the fiber <%s> has terminated but an attempt to resume it has been made",
            fiber->name);
}

void fiber_stack_protection(
        fiber_t *fiber,
        bool enable) {
    int stack_usage_flags = enable ? PROT_NONE : PROT_READ | PROT_WRITE;

    if (mprotect(
            fiber->stack_base,
            xalloc_get_page_size() * FIBER_GUARD_PAGES_COUNT,
            stack_usage_flags) != 0) {
        if (errno == ENOMEM) {
            fatal(TAG, "Unable to protect/unprotect fiber stack, review the value of /proc/sys/vm/max_map_count");
        }

        fatal(TAG, "Unable to protect/unprotect fiber stack");
    }
}

fiber_t *fiber_new(
        char *name,
        size_t name_len,
        size_t stack_size,
        fiber_start_fp_t *fiber_start_fp,
        void *user_data) {
    void *temp_swap_stack_ptr = NULL;
    size_t page_size = xalloc_get_page_size();

    // If user data are passed the start function must be passed too
    if (fiber_start_fp == NULL && user_data != NULL) {
        return NULL;
    }

    // Get the page size and calculate the guard length
    size_t guard_len = (FIBER_GUARD_PAGES_COUNT * page_size);

    // Round up the stack size to the nearest page size
    stack_size = (stack_size + page_size - 1) / page_size * page_size;

    // The end (beginning) of the stack is "protected" to catch any undesired access that tries to read data past the
    // end of the allocated stack memory (e.g. memory overflow). To protect the access a page is required therefore
    // at least 4 pages have to be allocated in addition to the guard length
    if (stack_size < guard_len + (page_size * 4)) {
        return NULL;
    }

#if defined(HAS_VALGRIND)
    // VALGRIND_STACK_REGISTER is not working as it should but valgrind automatically detects a stack change if the
    // stack pointer changes at least 2MB, therefore the stack size is increased to 2MB to make sure that valgrind
    // will detect the stack change
    stack_size = 2000000;
#endif

    // Calculate the stack size with the guard pages and allocate the stack
    size_t stack_size_with_guard = stack_size + guard_len;
    fiber_t *fiber = xalloc_alloc_zero(sizeof(fiber_t));
    void *stack_base = xalloc_alloc_aligned_zero(page_size, stack_size_with_guard);

    // Align the stack_pointer to 16 bytes and add some padding as required by the ABI adding abort to the stack
    void **stack_pointer = (void**)(((uintptr_t)stack_base + stack_size_with_guard) & -16L);

    // TODO: test fibers on AARCH64
#if defined(__aarch64__)
    stack_pointer--;
#endif

    *--stack_pointer = (void *)fiber_abort;
    *--stack_pointer = (void *)fiber_new_first_run;
    stack_pointer -= FIBER_CONTEXT_NUM_REGISTRIES;

    LOG_D(
            TAG,
            "Initializing new fiber <%s> with a stack of <%lu (%lu with guard pages)> bytes starting at <%p>",
            name,
            stack_size,
            stack_size_with_guard,
            stack_pointer);

    // Initialize the fiber context
    fiber->start_fp = fiber_start_fp;
    fiber->start_fp_user_data = user_data;
    fiber->stack_base = stack_base;
    fiber->stack_size = stack_size_with_guard;

    // Set the fiber additional parameters
    fiber->terminate = false;
    fiber->name = (char*)xalloc_alloc_zero(name_len + 1);
    strncpy(fiber->name, name, name_len);

    if (fiber_start_fp) {
        // Do a fist swap to initialize the fiber stack content
        fiber_new_first_run_fiber = fiber;
        fiber_new_first_run_fiber_start_fp = fiber_start_fp;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-local-addr"
        // CodeQL reports the below assignments as potentially dangerous because it is possible that the address of
        // assgined might be later overwritten, these are local (stack) variables and the address should never be used
        // outside of this function.
        // However, the address is used only to pass the address of the variable to the fiber_new_first_run function
        // and the fiber_new_first_run function will never use the address after the swap. This is required for the
        // fiber context (stack) swap implementation and can't be avoided.
        fiber_new_first_run_func_user_data = user_data;
        fiber_new_first_run_from = &temp_swap_stack_ptr;
        fiber_new_first_run_to = (void **)&stack_pointer;
#pragma GCC diagnostic pop

        fiber_context_swap(fiber_new_first_run_from, fiber_new_first_run_to);

        fiber_new_first_run_fiber = NULL;
        fiber_new_first_run_fiber_start_fp = NULL;
        fiber_new_first_run_func_user_data = NULL;
        fiber_new_first_run_from = NULL;
        fiber_new_first_run_to = NULL;
    }

    // The stack pointer HAS to be updated after fiber_context_swap because it will be updated with the new value
    // after the initial execution of the fiber
    fiber->stack_pointer = stack_pointer;

#if defined(HAS_VALGRIND)
    uintptr_t stack_base_addr = (uintptr_t)fiber->stack_base;
    uintptr_t stack_end_addr = stack_base_addr + fiber->stack_size;
    fiber->valgrind_stack_id = VALGRIND_STACK_REGISTER(stack_base_addr, stack_end_addr);
#endif

    fiber_stack_protection(fiber, true);

    return fiber;
}

void fiber_free(
        fiber_t *fiber) {
    fiber_stack_protection(fiber, false);

#if defined(HAS_VALGRIND)
    VALGRIND_STACK_DEREGISTER(fiber->valgrind_stack_id);
#endif

    xalloc_free(fiber->name);
    xalloc_free(fiber->stack_base);
    xalloc_free(fiber);
}
