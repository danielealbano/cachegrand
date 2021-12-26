/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include "exttypes.h"
#include "misc.h"
#include "xalloc.h"
#include "thread.h"
#include "log/log.h"
#include "fiber.h"

#include "worker_fiber_scheduler.h"

#define TAG "worker_fiber_scheduler"

typedef struct nested_fibers_stack nested_fibers_stack_t;
struct nested_fibers_stack {
    fiber_t **stack;
    int8_t index;
    int8_t size;
};

thread_local char nested_fibers_scheduler_name[255] = { 0 };
thread_local fiber_t nested_fibers_scheduler = { 0 };
thread_local nested_fibers_stack_t nested_fibers_stack = {
        .stack = NULL,
        .index = -1,
        .size = 0
};

void worker_fiber_scheduler_grow_nested_fibers_stack() {
    nested_fibers_stack.size++;
    nested_fibers_stack.stack = xalloc_realloc(
            nested_fibers_stack.stack,
            sizeof(fiber_t*) * nested_fibers_stack.size);
}

bool worker_fiber_scheduler_nested_fibers_stack_needs_grow() {
    return nested_fibers_stack.size == nested_fibers_stack.index + 1;
}

void worker_fiber_scheduler_switch_to(
        fiber_t *fiber) {
    fiber_t* previous_fiber;

    // TODO: pre-initialization of the stack, the scheduler context gets pushed to the stack, the operation shouldn't
    //       be done here but in an initialization call
    if (nested_fibers_stack.index == -1) {
        worker_fiber_scheduler_grow_nested_fibers_stack();

        snprintf(
                nested_fibers_scheduler_name,
                sizeof(nested_fibers_scheduler_name),
                "worker-scheduler");
        nested_fibers_scheduler.name = nested_fibers_scheduler_name;

        nested_fibers_stack.index++;
        nested_fibers_stack.stack[nested_fibers_stack.index] = &nested_fibers_scheduler;
    }

    if (worker_fiber_scheduler_nested_fibers_stack_needs_grow()) {
        worker_fiber_scheduler_grow_nested_fibers_stack();
    }

    // Fetch the previous fiber (or the scheduler)
    previous_fiber = nested_fibers_stack.stack[nested_fibers_stack.index];

    // Push the fiber onto the stack
    nested_fibers_stack.index++;
    nested_fibers_stack.stack[nested_fibers_stack.index] = fiber;

    LOG_D(TAG, "Switching from fiber <%s> to fiber <%s>", previous_fiber->name, fiber->name);

    // Switch to the new fiber
    fiber_context_swap(
            previous_fiber,
            fiber);

    LOG_D(TAG, "Switching back from fiber <%s> to fiber <%s>", fiber->name, previous_fiber->name);

    // Once the code switches back remove the fiber from the stack
    nested_fibers_stack.stack[nested_fibers_stack.index] = NULL;
    nested_fibers_stack.index--;
}

void worker_fiber_scheduler_switch_back() {
    assert(nested_fibers_stack.stack != NULL);
    assert(nested_fibers_stack.index > -1);

    // Switch back to the scheduler execution context, leaves in its hands to update the thread_current_fiber and
    // thread_scheduler_fiber tracking variables
    fiber_context_swap(
            nested_fibers_stack.stack[nested_fibers_stack.index],
            nested_fibers_stack.stack[nested_fibers_stack.index - 1]);
}

fiber_t *worker_fiber_scheduler_get_current() {
    assert(nested_fibers_stack.stack != NULL);
    assert(nested_fibers_stack.index > -1);

    return nested_fibers_stack.stack[nested_fibers_stack.index];
}
