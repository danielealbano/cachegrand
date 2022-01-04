/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <string.h>
#include <fatal.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "slab_allocator.h"
#include "config.h"
#include "log/log.h"
#include "fiber.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "worker/worker_common.h"
#include "worker/worker.h"
#include "worker/worker_op.h"
#include "fiber_scheduler.h"
#include "worker/network/worker_network.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "network/protocol/redis/network_protocol_redis.h"

#include "worker_network_op.h"

#define TAG "worker_network_op"

void worker_network_new_client_fiber_entrypoint(
        void *user_data) {
    worker_context_t *context = worker_context_get();

    network_channel_t *new_channel = user_data;
    worker_stats_t *stats = &context->stats.internal;

    // Updates the worker stats
    stats->network.active_connections++;
    stats->network.accepted_connections_total++;
    stats->network.accepted_connections_per_second++;

    // Should not access the listener_channel directly
    switch (new_channel->protocol) {
        default:
            // This can't really happen
            FATAL(
                    TAG,
                    "[FD:%5d][ACCEPT] Channel unknown protocol <%d>",
                    new_channel->fd,
                    new_channel->protocol);
            break;

        case NETWORK_PROTOCOLS_REDIS:
            res = network_protocol_redis_accept(
                    new_channel,
                    &worker_network_channel_user_data->protocol.context);
            break;
    }

    if (!res) {
        LOG_V(
                TAG,
                "[FD:%5d][ACCEPT] Protocol failed to handle accepted connection <%s>, closing connection",
                new_channel->fd,
                new_channel->address.str);
        worker_network_close(
                new_channel);
    } else {
        while(worker_network_protocol_process_events(
                new_channel,
                new_channel->user_data)) {
            // do nothing
        }
    }

    fiber_scheduler_terminate_current_fiber();
}

void worker_network_listeners_fiber_entrypoint(
        void* user_data) {
    network_channel_t *new_channel = NULL;
    network_channel_t *listener_channel = user_data;

    while((new_channel = worker_op_network_accept(
            listener_channel)) != NULL) {
        LOG_V(
                TAG,
                "[FD:%5d][ACCEPT] Listener <%s> accepting new connection from <%s>",
                listener_channel->fd,
                listener_channel->address.str,
                new_channel->address.str);

        fiber_scheduler_new_fiber(
                "worker-listener-client",
                strlen("worker-listener-client"),
                worker_network_new_client_fiber_entrypoint,
                (void *)new_channel);
    }

    if (new_channel == NULL) {
        LOG_W(
                TAG,
                "[FD:%5d][ACCEPT] Failed to accept a new connection coming from listener <%s>",
                listener_channel->fd,
                listener_channel->address.str);
    }

    // TODO: listener should be terminated, if failed for an error

    // Switch back to the scheduler, as the lister has been closed this fiber will never be invoked and will get freed
    fiber_scheduler_switch_back();
}
