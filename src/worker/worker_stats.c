/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
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
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"

// Needed to be defined there as there is a recursive dependency between worker_context and worker_stats
typedef struct worker_context worker_context_t;

#include "worker_stats.h"
#include "worker_context.h"

void worker_stats_publish(
        worker_stats_t* worker_stats_new,
        worker_stats_volatile_t* worker_stats_public) {
    clock_monotonic(&worker_stats_new->last_update_timestamp);

    memcpy((void*)&worker_stats_public->network, &worker_stats_new->network, sizeof(worker_stats_public->network));
    worker_stats_public->last_update_timestamp.tv_nsec = worker_stats_new->last_update_timestamp.tv_nsec;
    worker_stats_public->last_update_timestamp.tv_sec = worker_stats_new->last_update_timestamp.tv_sec;

    memset(&worker_stats_new->network.per_second, 0, sizeof(worker_stats_new->network.per_second));
    memset(&worker_stats_new->storage.per_second, 0, sizeof(worker_stats_new->storage.per_second));
}

bool worker_stats_should_publish(
        worker_stats_volatile_t* worker_stats_public) {
    struct timespec last_update_timestamp;

    clock_monotonic(&last_update_timestamp);

    return last_update_timestamp.tv_sec >= worker_stats_public->last_update_timestamp.tv_sec + WORKER_PUBLISH_STATS_DELAY_SEC;
}

worker_stats_t *worker_stats_get() {
    worker_context_t *context = worker_context_get();
    return &context->stats.internal;
}
