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

#include "worker_fiber_storage_db_keys_eviction.h"

#define TAG "worker_fiber_storage_db_initialize"

void worker_fiber_storage_db_keys_eviction_fiber_entrypoint(
        void* user_data) {
    worker_context_t *worker_context = worker_context_get();

    uint64_t last_run = clock_monotonic_coarse_int64_ms();
    while(worker_op_wait_ms(WORKER_FIBER_STORAGE_DB_KEYS_EVICTION_WAIT_LOOP_MS)) {
        // Check if limits have been passed
        if (likely(!storage_db_keys_eviction_should_run(worker_context->db))) {
            continue;
        }

        // To avoid to run the eviction too often, we calculate how close we are to the hard limits and then calculate
        // a wait time based on that, closer to the limits lower the wait time with being at 95% close to the hard limit
        // means run immediately.
        double close_to_hard_limit = storage_db_keys_eviction_calculate_close_to_hard_limit_percentage(
                worker_context->db);

        // Upscale close_to_hard_limit, 95% means 100%
        close_to_hard_limit = close_to_hard_limit * 1.05;

        // Ensure that close_to_hard_limit isn't greater than 1
        if (unlikely(close_to_hard_limit > 1)) {
            close_to_hard_limit = 1;
        }

        // Calculate the wait time
        uint64_t wait_time = (uint64_t) (1000 * (1 - close_to_hard_limit));

        // Check if enough time has passed from the last run
        if (likely(clock_monotonic_coarse_int64_ms() - last_run < wait_time)) {
            continue;
        }

        // Run the eviction
        storage_db_keys_eviction_run_worker(
                worker_context->db,
                worker_context->config->database->keys_eviction->only_ttl,
                worker_context->config->database->keys_eviction->policy);
    }
}