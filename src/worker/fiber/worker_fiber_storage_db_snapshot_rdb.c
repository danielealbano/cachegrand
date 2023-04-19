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
    bool result;
    bool last_block = false;
    worker_context_t *worker_context = worker_context_get();
    storage_db_t *db = worker_context->db;

    while(true) {
        if (!worker_is_running(worker_context) || db->snapshot.next_run_time_ms == 0) {
            worker_op_wait_ms(WORKER_FIBER_STORAGE_DB_SNAPSHOT_RDB_WAIT_LOOP_MS);
            continue;
        }

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

            // Check if there is any configured constraint to run the snapshot
            if (!storage_db_snapshot_enough_keys_data_changed(db)) {
                // If there aren't enough keys or data changed, skip the run and wait for the next one
                storage_db_snapshot_skip_run(db);
                continue;
            }

            // Ensure that the snapshot is prepared to run
            if (!storage_db_snapshot_rdb_ensure_prepared(db)) {
                continue;
            }
        }

        // Check if it's possible to acquire the lock for the snapshot, if not retry later
        if (!storage_db_snapshot_lock_try_acquire(db)) {
            // To avoid a busy loop, yield the fiber
            worker_op_wait_ms(0);
            continue;
        }

        // Process a block
        result = storage_db_snapshot_rdb_process_block(db, &last_block);

        // If the block was processed successfully and was the last one, mark the snapshot as not running anymore
        if (result && last_block) {
            storage_db_snapshot_mark_as_being_finalized(db);
        }

        // If the previous block was processed successfully, process the queue of entry indexes to be deleted
        // As the snapshot is marked as being finalized once the last block is processed, no more entries will be
        // appended
        if (result) {
            result = storage_db_snapshot_rdb_process_entry_index_to_be_deleted_queue(db);
        }

        // Check if it's time to report the progress
        if (result && (last_block || storage_db_snapshot_should_report_progress(db))) {
            storage_db_snapshot_report_progress(db);
        }

        // If the queue was processed successfully, check if the previously processed block was the last one
        if (result && last_block) {
            result = storage_db_snapshot_rdb_completed_successfully(db);
        }

        // If the snapshot failed, mark it as failed
        if (result == false) {
            // Flush the queue of entry indexes to be deleted
            storage_db_snapshot_rdb_flush_entry_index_to_be_deleted_queue(db);

            // Mark the snapshot as failed
            storage_db_snapshot_failed(db);
        }

        // Release the lock
        storage_db_snapshot_release_lock(db);

        // To avoid a busy loop, if there are more than 2 workers, wait a bit before checking again otherwise just
        // yield the fiber
        worker_op_wait_ms(worker_context->workers_count > 2
            ? WORKER_FIBER_STORAGE_DB_SNAPSHOT_RDB_WAIT_LOOP_MS
            : 0);
    }

    // Switch back
    fiber_scheduler_switch_back();
}
