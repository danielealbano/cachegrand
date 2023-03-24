/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#if defined(DEBUG) &&  __has_include(<valgrind/valgrind.h>)
#include <valgrind/valgrind.h>
#define HAS_VALGRIND
#endif

#include "misc.h"
#include "xalloc.h"
#include "log/log.h"
#include "fatal.h"
#include "fiber.h"
#include "fiber_scheduler.h"
#include "intrinsics.h"

#define TAG "fiber_scheduler"

thread_local char nested_fibers_scheduler_name[255] = { 0 };
thread_local fiber_t nested_fibers_scheduler = { 0 };
thread_local fiber_scheduler_stack_t fiber_scheduler_stack = {
        .list = NULL,
        .index = -1,
        .size = 0
};

void fiber_scheduler_free() {
    if (fiber_scheduler_stack.list) {
        xalloc_free(fiber_scheduler_stack.list);
        fiber_scheduler_stack.list = NULL;
    }

    fiber_scheduler_stack.index = -1;
    fiber_scheduler_stack.size = 0;
}

void fiber_scheduler_grow_stack() {
    if (fiber_scheduler_stack.size == FIBER_SCHEDULER_STACK_MAX_SIZE) {
        FATAL(
                TAG,
                "Trying to grow the fiber scheduler stack over the allowed size of <%d>, most likely it's a bug",
                FIBER_SCHEDULER_STACK_MAX_SIZE);
    }

    fiber_scheduler_stack.size++;
    fiber_scheduler_stack.list = xalloc_realloc(
            fiber_scheduler_stack.list,
            sizeof(fiber_t*) * fiber_scheduler_stack.size);
}

bool fiber_scheduler_stack_needs_growth() {
    return fiber_scheduler_stack.size == fiber_scheduler_stack.index + 1;
}

void fiber_scheduler_new_fiber_entrypoint(
        void *fiber_scheduler_new_fiber_user_data_void) {
    fiber_scheduler_new_fiber_user_data_t *fiber_scheduler_new_fiber_user_data = fiber_scheduler_new_fiber_user_data_void;

    fiber_scheduler_new_fiber_user_data->caller_entrypoint_fp(
            fiber_scheduler_new_fiber_user_data->caller_user_data);

    fiber_t *current = fiber_scheduler_get_current();

    if (current->terminate == false) {
        LOG_E(
                TAG,
                "Internal error, the fiber <%s> is terminating but the termination hasn't been requested",
                current->name);
        current->terminate = true;
    }

    while(true) {
        fiber_scheduler_switch_back();
        // Although a terminated fiber can't be switched back because the allocated memory is freed and therefore would
        // normally be re-used, to avoid any risk of potential unnoticed bugs a FATAL is introduced here to catch any
        // case where a terminated fiber where the related memory hasn't been reused gets switched to.
        // In this case it's important to hard-fail as any execution would lead to a fatal crash anyway.
        FATAL(TAG, "Switched back to a terminated fiber, unable to continue");
    }
}

fiber_t* fiber_scheduler_new_fiber(
        char *name,
        size_t name_len,
        fiber_scheduler_entrypoint_fp_t* entrypoint_fp,
        void *user_data) {

    fiber_scheduler_new_fiber_user_data_t fiber_scheduler_new_fiber_user_data = {
            .caller_entrypoint_fp = entrypoint_fp,
            .caller_user_data = user_data
    };

    fiber_t* fiber = fiber_new(
            name,
            name_len,
            FIBER_SCHEDULER_STACK_SIZE,
            fiber_scheduler_new_fiber_entrypoint,
            &fiber_scheduler_new_fiber_user_data);

    fiber_scheduler_switch_to(fiber);

    return fiber;
}

void fiber_scheduler_terminate_current_fiber() {
    fiber_t *fiber = fiber_scheduler_get_current();

    fiber->terminate = true;
}

void fiber_scheduler_switch_to(
        fiber_t *fiber) {
    fiber_t* previous_fiber;

    // TODO: pre-initialization of the stack, the scheduler context gets pushed to the stack, the operation shouldn't
    //       be done here but in an initialization call
    if (fiber_scheduler_stack.index == -1) {
        fiber_scheduler_grow_stack();

        snprintf(
                nested_fibers_scheduler_name,
                sizeof(nested_fibers_scheduler_name),
                FIBER_SCHEDULER_FIBER_NAME);
        nested_fibers_scheduler.name = nested_fibers_scheduler_name;
#if DEBUG == 1
        nested_fibers_scheduler.switched_back_on.file = (char*)CACHEGRAND_SRC_PATH;
        nested_fibers_scheduler.switched_back_on.line = __LINE__;
        nested_fibers_scheduler.switched_back_on.func = "fiber_scheduler_switch_to";
#endif

        fiber_scheduler_stack.index++;
        fiber_scheduler_stack.list[fiber_scheduler_stack.index] = &nested_fibers_scheduler;
    }

    if (fiber_scheduler_stack_needs_growth()) {
        fiber_scheduler_grow_stack();
    }

    // Fetch the previous fiber (or the scheduler)
    previous_fiber = fiber_scheduler_stack.list[fiber_scheduler_stack.index];

    // Push the fiber onto the stack
    fiber_scheduler_stack.index++;
    fiber_scheduler_stack.list[fiber_scheduler_stack.index] = fiber;

    LOG_D(TAG, "Switching from fiber <%s> to fiber <%s>, file <%s:%d>, function <%s>",
          previous_fiber->name,
          fiber->name,
          fiber->switched_back_on.file,
          fiber->switched_back_on.line,
          fiber->switched_back_on.func);

    // Switch to the new fiber
    fiber_context_swap(
            &previous_fiber->stack_pointer,
            &fiber->stack_pointer);

    LOG_D(TAG, "Switching back from fiber <%s>, file <%s:%d>, to fiber <%s>, file <%s:%d>",
          fiber->name,
          fiber->switched_back_on.file,
          fiber->switched_back_on.line,
          previous_fiber->name,
          previous_fiber->switched_back_on.file,
          previous_fiber->switched_back_on.line);

    // Once the code switches back remove the fiber from the stack
    fiber_scheduler_stack.list[fiber_scheduler_stack.index] = NULL;
    fiber_scheduler_stack.index--;

    if (fiber->terminate) {
        LOG_D(TAG, "Fiber marked for termination, cleaning up");
        fiber_free(fiber);
    }
}

#if DEBUG == 1
void fiber_scheduler_switch_back_internal(
    const char *file,
    int line,
    const char *func) {
#else
void fiber_scheduler_switch_back() {
#endif
    assert(fiber_scheduler_stack.list != NULL);
    assert(fiber_scheduler_stack.index > -1);

    fiber_t *current_fiber = fiber_scheduler_stack.list[fiber_scheduler_stack.index];
#if DEBUG == 1
    current_fiber->switched_back_on.file = file;
    current_fiber->switched_back_on.line = line;
    current_fiber->switched_back_on.func = func;
#endif

    // Switch back to the scheduler execution context, leaves in its hands to update the thread_current_fiber and
    // thread_scheduler_fiber tracking variables
    fiber_context_swap(
            &fiber_scheduler_stack.list[fiber_scheduler_stack.index]->stack_pointer,
            &fiber_scheduler_stack.list[fiber_scheduler_stack.index - 1]->stack_pointer);
}

fiber_t *fiber_scheduler_get_current() {
    assert(fiber_scheduler_stack.list != NULL);
    assert(fiber_scheduler_stack.index > -1);

    return fiber_scheduler_stack.list[fiber_scheduler_stack.index];
}

void fiber_scheduler_set_error(int error_number) {
    assert(error_number >= 0);

    fiber_t *fiber = fiber_scheduler_get_current();
    errno = fiber->error_number = error_number;
}

int fiber_scheduler_get_error() {
    fiber_t *fiber = fiber_scheduler_get_current();
    return fiber->error_number;
}

bool fiber_scheduler_has_error() {
    fiber_t *fiber = fiber_scheduler_get_current();
    return fiber->error_number != 0;
}

void fiber_scheduler_reset_error() {
    fiber_scheduler_set_error(0);
}
