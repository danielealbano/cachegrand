/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <stddef.h>

#include "exttypes.h"
#include "misc.h"
#include "clock.h"
#include "fiber/fiber.h"
#include "log/log.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "config.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "fiber/fiber_scheduler.h"
#include "worker/worker_op.h"

#include "worker_fiber_storage_db_initialize.h"

#define TAG "worker_fiber_storage_db_initialize"

void worker_fiber_storage_db_initialize_fiber_entrypoint(
        void* user_data) {
    bool *storage_db_loaded = (bool*)user_data;
    worker_context_t *worker_context = worker_context_get();

    // Initialize the storage db
    if (!storage_db_open(worker_context->db)) {
        FATAL(TAG, "Unable to open the database failed");
    }

    // TODO: Load the initial snapshot if necessary

    // Set the next snapshot run time and add 100ms to have a bit more padding during the startup
    storage_db_config_t *storage_db_config = worker_context->db->config;
    worker_context->db->snapshot.next_run_time_ms =
            clock_monotonic_coarse_int64_ms() +
            storage_db_config->snapshot.interval_ms +
            100;

    // Mark the storage db as loaded
    *storage_db_loaded = true;

    // Mark the current fiber as terminated
    fiber_scheduler_terminate_current_fiber();
}
