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

#include "exttypes.h"
#include "misc.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "fiber/fiber.h"
#include "fiber/fiber_scheduler.h"
#include "config.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "storage/db/storage_db_snapshot.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker_op.h"

#include "worker_fiber_storage_db_snapshot_rdb.h"

#define TAG "worker_fiber_storage_db_snapshot"

void worker_fiber_storage_db_snapshot_rdb_fiber_entrypoint(
        void* user_data) {
    bool last_block = false;
    worker_context_t *worker_context = worker_context_get();
    storage_db_t *db = worker_context->db;

    // If the time is set to zero something went wrong during the initialization and the code shouldn't even get here
    assert(db->snapshot.next_run_time_ms > 0);

    while(true) {
        // If the snapshot is not running, wait and then check if it's time to run it and if yes ensure that the
        // snapshot is prepared to run
        if (!db->snapshot.running) {
            if (!worker_op_wait_ms(WORKER_FIBER_STORAGE_DB_SNAPSHOT_RDB_WAIT_LOOP_MS)) {
                break;
            }

            // Check if limits have been passed
            if (likely(!storage_db_snapshot_should_run(db))) {
                continue;
            }

            // Ensure that the snapshot is prepared to run
            if (!storage_db_snapshot_rdb_ensure_prepared(db)) {
                continue;
            }
        }

        // Check if it's possible to acquire the lock for the snapshot, if not retry later
        if (!storage_db_snapshot_lock_try_acquire(db)) {
            continue;
        }

        // Process a block
        storage_db_snapshot_rdb_process_block(db, &last_block);

        // Release the lock
        storage_db_snapshot_release_lock(db);
    }
}
