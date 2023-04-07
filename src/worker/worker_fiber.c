/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "config.h"
#include "log/log.h"
#include "fiber/fiber.h"
#include "fiber/fiber_scheduler.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"

#include "worker_fiber.h"

#define TAG "worker_fiber"

bool worker_fiber_init(
        worker_context_t* worker_context) {
    worker_context->fibers = double_linked_list_init();

    if (unlikely(worker_context->fibers == NULL)) {
        LOG_E(TAG, "Failed to initialize the fibers list");
        return false;
    }

    return true;
}

bool worker_fiber_register(
        worker_context_t* worker_context,
        char *fiber_name,
        fiber_scheduler_entrypoint_fp_t *fiber_entrypoint,
        fiber_scheduler_new_fiber_user_data_t *fiber_user_data) {
    // Pre-allocate the item so that if the operation fails the fiber will not be started
    double_linked_list_item_t *item = double_linked_list_item_init();
    if (unlikely(item == NULL)) {
        LOG_E(TAG, "Failed to register fiber <%s>", fiber_name);
        return false;
    }

    // Setup the fiber
    fiber_t *fiber = fiber_scheduler_new_fiber(
            fiber_name,
            strlen(fiber_name),
            fiber_entrypoint,
            fiber_user_data);

    // Check if it failed
    if (unlikely(fiber == NULL)) {
        LOG_E(TAG, "Failed to register fiber <%s>", fiber_name);
        double_linked_list_item_free(item);
        return false;
    }

    // Set the data and append it to the list of fibers
    item->data = fiber;
    double_linked_list_push_item(worker_context->fibers, item);

    return true;
}

void worker_fiber_free(
        worker_context_t* worker_context) {
    double_linked_list_item_t *item;
    while ((item = double_linked_list_pop_item(worker_context->fibers)) != NULL) {
        fiber_t *fiber = item->data;
        fiber_free(fiber);
        double_linked_list_item_free(item);
    }

    double_linked_list_free(worker_context->fibers);
    worker_context->fibers = NULL;
}
