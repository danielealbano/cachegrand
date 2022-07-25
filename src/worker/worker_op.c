/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>

#include "exttypes.h"
#include "misc.h"
#include "clock.h"
#include "fiber.h"
#include "log/log.h"
#include "spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "config.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "fiber_scheduler.h"
#include "worker/worker_op.h"

#define TAG "worker_op"

worker_op_timer_fp_t* worker_op_timer;

void worker_timer_fiber_entrypoint(
        void *user_data) {
    worker_context_t* worker_context = worker_context_get();

    while(worker_op_timer(0, WORKER_TIMER_LOOP_MS * 1000000l)) {
        // The condition checked below is mostly for the tests, as there are some simple tests that should setup the
        // storage_db otherwise
        if (worker_context->db) {
            storage_db_worker_garbage_collect_deleting_entry_index_when_no_readers(worker_context->db);
        }
    }
}

void worker_timer_setup(
        worker_context_t* worker_context) {
    worker_context->fibers.timer_fiber = fiber_scheduler_new_fiber(
            "worker-timer",
            sizeof("worker-timer") - 1,
            worker_timer_fiber_entrypoint,
            NULL);
}
