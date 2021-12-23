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

thread_local fiber_t *thread_current_fiber = NULL;
thread_local fiber_t thread_scheduler_fiber = { 0 };

void worker_fiber_scheduler_switch_to(
        fiber_t *fiber) {
    LOG_D(TAG, "Switching to fiber <%s>", fiber->name);

    // Set the current fiber to keep track of the execution
    thread_current_fiber = fiber;

    // Switch to the new fiber
    fiber_context_swap(&thread_scheduler_fiber, fiber);

    LOG_D(TAG, "Switching back to scheduler from fiber <%s>", fiber->name);

    // Once the code switches back it sets the current fiber to null
    thread_current_fiber = NULL;
}

void worker_fiber_scheduler_switch_back() {
    // Switch back to the scheduler execution context, leaves in its hands to update the thread_current_fiber and
    // thread_scheduler_fiber tracking variables
    fiber_context_swap(thread_current_fiber, &thread_scheduler_fiber);
}

fiber_t *worker_fiber_scheduler_get_current() {
    return thread_current_fiber;
}

void worker_fiber_scheduler_ensure_in_fiber() {
    // This function is used only in debug builds as it's relies on assert, it's used to ensure that the caller is
    // being invoked within a fiber as it expects to use fibers related functions (e.g. switch back to the scheduler
    // once the operations are completed)
    assert(thread_current_fiber != NULL);
}
