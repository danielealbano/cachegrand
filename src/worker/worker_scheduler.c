#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "fiber.h"
#include "misc.h"

#include "worker_scheduler.h"

thread_local fiber_t scheduler_worker_main_context = {0 };
thread_local fiber_t *scheduler_running_fiber = NULL;

bool worker_scheduler_is_fiber_running() {
    return scheduler_running_fiber != NULL;
}

fiber_t *worker_scheduler_get_running_fiber() {
    return scheduler_running_fiber;
}

void worker_scheduler_switch_to_fiber(fiber_t* fiber) {
    scheduler_running_fiber = fiber;
    fiber_context_swap(&scheduler_worker_main_context, scheduler_running_fiber);
}

void worker_scheduler_switch_back() {
    fiber_t *scheduler_switch_to_fiber_internal = scheduler_running_fiber;
    scheduler_running_fiber = NULL;
    fiber_context_swap(scheduler_switch_to_fiber_internal, &scheduler_worker_main_context);
}
