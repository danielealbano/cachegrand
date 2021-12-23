/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
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

void fiber_stack_protection(
        fiber_t *fiber,
        bool enable) {
    int stack_usage_flags = enable ? PROT_NONE : PROT_READ | PROT_WRITE;

    if (mprotect(fiber->stack_base, xalloc_get_page_size(), stack_usage_flags) != 0) {
        if (errno == ENOMEM) {
            fatal(TAG, "Unable to protect/unprotect fiber stack, review the value of /proc/sys/vm/max_map_count");
        }

        fatal(TAG, "Unable to protect/unprotect fiber stack");
    }
}

fiber_t *fiber_new(
        char *name,
        size_t stack_size,
        fiber_start_fp_t *fiber_start_fp,
        void *user_data) {
    assert(fiber_start_fp != NULL);

    size_t page_size = xalloc_get_page_size();

    // The end (beginning) of the stack is "protected" to catch any undesired access that tries to read data past the
    // end of the allocated stack memory (e.g. memory overflow). To protect the access a page is required therefore
    // at least 2 pages have to be allocated, one minimum for the stack and one to protect the stack.
    if (stack_size < page_size * 2) {
        return NULL;
    }

    fiber_t *fiber = xalloc_alloc_zero(sizeof(fiber_t));
    void *stack_base = xalloc_alloc_aligned_zero(page_size, stack_size);

    // Align the stack_pointer to 16 bytes and leave the 128 bytes red zone free as per ABI requirements
    void* stack_pointer = (void*)((uintptr_t)(stack_base + stack_size) & -16L) - 128;

    // Need room on the stack as we push/pop a return address to jump to our function
    stack_pointer -= sizeof(void*) * 1;

    LOG_D(
            TAG,
            "Initializing new fiber <%s> with a stack of <%lu> bytes starting at <%p>",
            name,
            stack_size,
            stack_pointer);

    fiber->start_fp = fiber_start_fp;
    fiber->start_fp_user_data = user_data;
    fiber->stack_size = stack_size;
    fiber->stack_base = stack_base;
    fiber->stack_pointer = stack_pointer;

    // Set the fiber name
    fiber->name = name;

    // Set the initial fp and rsp of the fiber
    fiber->context.rip = fiber->start_fp; // this or the stack_base? who knows :|
    fiber->context.rsp = fiber->stack_pointer;

    fiber_stack_protection(fiber, true);

    return fiber;
}

void fiber_free(
        fiber_t *fiber) {
    fiber_stack_protection(fiber, false);

    xalloc_free(fiber->stack_base);
    xalloc_free(fiber);
}
