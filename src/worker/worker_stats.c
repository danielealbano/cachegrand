/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
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

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "config.h"
#include "fiber.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "slab_allocator.h"
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
#include "program.h"

// Needed to be defined there as there is a recursive dependency between worker_context and worker_stats
typedef struct worker_context worker_context_t;

#include "worker_stats.h"
#include "worker_context.h"

void worker_stats_publish(
        worker_stats_t* worker_stats_internal,
        worker_stats_volatile_t* worker_stats_public,
        bool only_total) {
    clock_realtime((timespec_t *)&worker_stats_public->total_last_update_timestamp);

    worker_stats_public->started_on_timestamp.tv_nsec = worker_stats_internal->started_on_timestamp.tv_nsec;
    worker_stats_public->started_on_timestamp.tv_sec = worker_stats_internal->started_on_timestamp.tv_sec;

    if (only_total) {
        memcpy(
                (void*)&worker_stats_public->network.total,
                &worker_stats_internal->network.total,
                sizeof(worker_stats_public->network.total));
        memcpy(
                (void*)&worker_stats_public->storage.total,
                &worker_stats_internal->storage.total,
                sizeof(worker_stats_public->storage.total));
    } else {
        worker_stats_public->per_minute_last_update_timestamp.tv_nsec =
                worker_stats_public->total_last_update_timestamp.tv_nsec;
        worker_stats_public->per_minute_last_update_timestamp.tv_sec =
                worker_stats_public->total_last_update_timestamp.tv_sec;

        memcpy(
                (void*)&worker_stats_public->network,
                &worker_stats_internal->network,
                sizeof(worker_stats_public->network));
        memcpy(
                (void*)&worker_stats_public->storage,
                &worker_stats_internal->storage,
                sizeof(worker_stats_public->storage));

        memset(&worker_stats_internal->network.per_minute, 0, sizeof(worker_stats_internal->network.per_minute));
        memset(&worker_stats_internal->storage.per_minute, 0, sizeof(worker_stats_internal->storage.per_minute));
    }
}

bool worker_stats_should_publish_after_interval(
        worker_stats_volatile_t* worker_stats_public,
        int interval) {
    struct timespec last_update_timestamp;

    clock_realtime(&last_update_timestamp);

    bool res = last_update_timestamp.tv_sec >=
        worker_stats_public->per_minute_last_update_timestamp.tv_sec + interval;
    return res;
}

worker_stats_t *worker_stats_get() {
    worker_context_t *context = worker_context_get();
    return &context->stats.internal;
}

worker_stats_t *worker_stats_aggregate(
        worker_stats_t *aggregated_stats) {
    program_context_t *program_context = program_get_context();

    // TODO: The access should be synced (e.g. with a spinlock) as the workers might decide to update the stats while
    //       they are being read providing potentially messed up data
    for(uint32_t index = 0; index < program_context->workers_count; index++) {
        worker_stats_volatile_t *worker_stats_shared = &program_context->workers_context[index].stats.shared;

        aggregated_stats->network.total.received_packets +=
                worker_stats_shared->network.total.received_packets;
        aggregated_stats->network.total.received_data +=
                worker_stats_shared->network.total.received_data;
        aggregated_stats->network.total.sent_packets +=
                worker_stats_shared->network.total.sent_packets;
        aggregated_stats->network.total.sent_data +=
                worker_stats_shared->network.total.sent_data;
        aggregated_stats->network.total.accepted_connections +=
                worker_stats_shared->network.total.accepted_connections;
        aggregated_stats->network.total.active_connections +=
                worker_stats_shared->network.total.active_connections;
        aggregated_stats->network.total.accepted_tls_connections +=
                worker_stats_shared->network.total.accepted_tls_connections;
        aggregated_stats->network.total.active_tls_connections +=
                worker_stats_shared->network.total.active_tls_connections;
        aggregated_stats->network.per_minute.received_packets +=
                worker_stats_shared->network.per_minute.received_packets;
        aggregated_stats->network.per_minute.received_data +=
                worker_stats_shared->network.per_minute.received_data;
        aggregated_stats->network.per_minute.sent_data +=
                worker_stats_shared->network.per_minute.sent_data;
        aggregated_stats->network.per_minute.sent_packets +=
                worker_stats_shared->network.per_minute.sent_packets;
        aggregated_stats->network.per_minute.accepted_connections +=
                worker_stats_shared->network.per_minute.accepted_connections;
        aggregated_stats->network.per_minute.accepted_tls_connections +=
                worker_stats_shared->network.per_minute.accepted_tls_connections;

        aggregated_stats->storage.total.written_data +=
                worker_stats_shared->storage.total.written_data;
        aggregated_stats->storage.total.write_iops +=
                worker_stats_shared->storage.total.write_iops;
        aggregated_stats->storage.total.read_data +=
                worker_stats_shared->storage.total.read_data;
        aggregated_stats->storage.total.read_iops +=
                worker_stats_shared->storage.total.read_iops;
        aggregated_stats->storage.total.open_files +=
                worker_stats_shared->storage.total.open_files;
        aggregated_stats->storage.per_minute.written_data +=
                worker_stats_shared->storage.per_minute.written_data;
        aggregated_stats->storage.per_minute.write_iops +=
                worker_stats_shared->storage.per_minute.write_iops;
        aggregated_stats->storage.per_minute.read_data +=
                worker_stats_shared->storage.per_minute.read_data;
        aggregated_stats->storage.per_minute.read_iops +=
                worker_stats_shared->storage.per_minute.read_iops;

        if (worker_stats_shared->total_last_update_timestamp.tv_sec >
            aggregated_stats->total_last_update_timestamp.tv_sec) {
            aggregated_stats->total_last_update_timestamp.tv_sec =
                    worker_stats_shared->total_last_update_timestamp.tv_sec;
        }

        if (worker_stats_shared->per_minute_last_update_timestamp.tv_sec >
            aggregated_stats->per_minute_last_update_timestamp.tv_sec) {
            aggregated_stats->per_minute_last_update_timestamp.tv_sec =
                    worker_stats_shared->per_minute_last_update_timestamp.tv_sec;
        }
    }

    aggregated_stats->started_on_timestamp.tv_sec =
            program_context->workers_context[0].stats.shared.started_on_timestamp.tv_sec;

    return aggregated_stats;
}