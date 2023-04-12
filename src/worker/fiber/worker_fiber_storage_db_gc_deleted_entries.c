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
#include "log/log.h"
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
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "fiber/fiber_scheduler.h"
#include "worker/worker_op.h"

#include "worker_fiber_storage_db_gc_deleted_entries.h"

#define TAG "worker_fiber_gc_deleted_storage_db_entries"

void worker_fiber_storage_db_gc_deleted_entries_fiber_entrypoint(
        void *user_data) {
    worker_context_t* worker_context = worker_context_get();

    while(worker_op_wait_ms(WORKER_FIBER_STORAGE_DB_GC_DELETED_STORAGE_DB_ENTRIES_WAIT_LOOP_MS)) {
        if (!worker_is_running(worker_context)) {
            continue;
        }

        // The condition checked below is mostly for the tests, as there are some simple tests that should setup the
        // storage_db otherwise
        if (likely(worker_context->db)) {
            storage_db_worker_garbage_collect_deleting_entry_index_when_no_readers(worker_context->db);
        }
    }

    // Switch back
    fiber_scheduler_switch_back();
}
