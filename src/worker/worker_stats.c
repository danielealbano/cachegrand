/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <clock.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stddef.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "transaction.h"
#include "config.h"
#include "fiber/fiber.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "memory_allocator/ffma.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "signal_handler_thread.h"
#include "epoch_gc.h"
#include "epoch_gc_worker.h"
#include "program.h"

// Needed to be defined there as there is a recursive dependency between worker_context and worker_stats
typedef struct worker_context worker_context_t;

#include "worker_stats.h"
#include "worker_context.h"

void worker_stats_publish(
        worker_stats_t* worker_stats_internal,
        worker_stats_volatile_t* worker_stats_public) {
    clock_realtime((timespec_t *)&worker_stats_public->last_update_timestamp);

    worker_stats_public->started_on_timestamp.tv_nsec = worker_stats_internal->started_on_timestamp.tv_nsec;
    worker_stats_public->started_on_timestamp.tv_sec = worker_stats_internal->started_on_timestamp.tv_sec;

    memcpy(
            (void*)&worker_stats_public->network,
            &worker_stats_internal->network,
            sizeof(worker_stats_public->network));
    memcpy(
            (void*)&worker_stats_public->storage,
            &worker_stats_internal->storage,
            sizeof(worker_stats_public->storage));
}

bool worker_stats_should_publish_totals_after_interval(
        worker_stats_volatile_t* worker_stats_public) {
    struct timespec last_update_timestamp;

    clock_realtime(&last_update_timestamp);

    bool res = last_update_timestamp.tv_sec >=
               worker_stats_public->last_update_timestamp.tv_sec + 1;

    return res;
}

worker_stats_t *worker_stats_get_internal_current() {
    worker_context_t *context = worker_context_get();
    return &context->stats.internal;
}

bool worker_stats_get_shared_by_index(
        uint32_t index,
        worker_stats_t *return_worker_stats) {
    program_context_t *program_context = program_get_context();

    if (index >= program_context->workers_count) {
        return false;
    }

    memcpy(
            return_worker_stats,
            (worker_stats_t *)&program_context->workers_context[index].stats.shared,
            sizeof(worker_stats_t));

    return true;
}

worker_stats_t *worker_stats_aggregate(
        worker_stats_t *aggregated_stats) {
    program_context_t *program_context = program_get_context();

    // TODO: The access should be synced (e.g. with a spinlock) as the workers might decide to update the stats while
    //       they are being read providing potentially messed up data
    for(uint32_t index = 0; index < program_context->workers_count; index++) {
        worker_stats_volatile_t *worker_stats_shared = &program_context->workers_context[index].stats.shared;

        aggregated_stats->network.received_packets +=
                worker_stats_shared->network.received_packets;
        aggregated_stats->network.received_data +=
                worker_stats_shared->network.received_data;
        aggregated_stats->network.sent_packets +=
                worker_stats_shared->network.sent_packets;
        aggregated_stats->network.sent_data +=
                worker_stats_shared->network.sent_data;
        aggregated_stats->network.accepted_connections +=
                worker_stats_shared->network.accepted_connections;
        aggregated_stats->network.active_connections +=
                worker_stats_shared->network.active_connections;
        aggregated_stats->network.accepted_tls_connections +=
                worker_stats_shared->network.accepted_tls_connections;
        aggregated_stats->network.active_tls_connections +=
                worker_stats_shared->network.active_tls_connections;

        aggregated_stats->storage.written_data +=
                worker_stats_shared->storage.written_data;
        aggregated_stats->storage.write_iops +=
                worker_stats_shared->storage.write_iops;
        aggregated_stats->storage.read_data +=
                worker_stats_shared->storage.read_data;
        aggregated_stats->storage.read_iops +=
                worker_stats_shared->storage.read_iops;
        aggregated_stats->storage.open_files +=
                worker_stats_shared->storage.open_files;

        if (worker_stats_shared->last_update_timestamp.tv_sec >
            aggregated_stats->last_update_timestamp.tv_sec) {
            aggregated_stats->last_update_timestamp.tv_sec =
                    worker_stats_shared->last_update_timestamp.tv_sec;
            aggregated_stats->last_update_timestamp.tv_nsec =
                    worker_stats_shared->last_update_timestamp.tv_nsec;
        } else if (worker_stats_shared->last_update_timestamp.tv_sec ==
                   aggregated_stats->last_update_timestamp.tv_sec &&
                   worker_stats_shared->last_update_timestamp.tv_nsec >
                   aggregated_stats->last_update_timestamp.tv_nsec) {
            aggregated_stats->last_update_timestamp.tv_nsec =
                    worker_stats_shared->last_update_timestamp.tv_nsec;
        }
    }

    aggregated_stats->started_on_timestamp.tv_sec =
            program_context->workers_context[0].stats.shared.started_on_timestamp.tv_sec;

    return aggregated_stats;
}