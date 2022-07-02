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
#include "clock.h"
#include "spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "slab_allocator.h"
#include "config.h"
#include "log/log.h"
#include "fiber.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"
#include "worker/worker_op.h"
#include "fiber_scheduler.h"
#include "network/network.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "network/protocol/redis/network_protocol_redis.h"

#include "worker_network_op.h"

#define TAG "worker_network_op"

// Network operations
worker_op_network_channel_new_fp_t* worker_op_network_channel_new;
worker_op_network_channel_multi_new_fp_t* worker_op_network_channel_multi_new;
worker_op_network_channel_multi_get_fp_t* worker_op_network_channel_multi_get;
worker_op_network_channel_size_fp_t* worker_op_network_channel_size;
worker_op_network_channel_free_fp_t* worker_op_network_channel_free;
worker_op_network_accept_fp_t* worker_op_network_accept;
worker_op_network_receive_fp_t* worker_op_network_receive;
worker_op_network_send_fp_t* worker_op_network_send;
worker_op_network_close_fp_t* worker_op_network_close;

// TODO: the listener and accept operations should be refactored to split them in an user frontend operation and in an
//       internal operation like for all the other ops (recv, send, close, etc.)
void worker_network_listeners_initialize(
        uint8_t core_index,
        config_network_t *config_network,
        network_channel_t **listeners,
        uint8_t *listeners_count) {
    network_channel_listener_new_callback_user_data_t listener_new_cb_user_data = { 0 };

    listener_new_cb_user_data.core_index = core_index;

    // With listeners = NULL, the number of needed listeners will be enumerated and listeners_count
    // increased as needed
    listener_new_cb_user_data.listeners = NULL;
    for(int protocol_index = 0; protocol_index < config_network->protocols_count; protocol_index++) {
        config_network_protocol_t *config_network_protocol = &config_network->protocols[protocol_index];
        for(int binding_index = 0; binding_index < config_network_protocol->bindings_count; binding_index++) {
            if (network_channel_listener_new(
                    config_network_protocol->bindings[binding_index].host,
                    config_network_protocol->bindings[binding_index].port,
                    config_network->listen_backlog,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data) == false) {
            }
        }
    }

    // Allocate the needed listeners and reset listeners_count
    listener_new_cb_user_data.listeners =
            worker_op_network_channel_multi_new(listener_new_cb_user_data.listeners_count);
    listener_new_cb_user_data.network_channel_size = worker_op_network_channel_size();
    listener_new_cb_user_data.listeners_count = 0;

    // Allocate the listeners (with the correct protocol config)
    for(int protocol_index = 0; protocol_index < config_network->protocols_count; protocol_index++) {
        network_protocols_t network_protocol;

        config_network_protocol_t *config_network_protocol = &config_network->protocols[protocol_index];
        switch(config_network_protocol->type) {
            default:
            case CONFIG_PROTOCOL_TYPE_REDIS:
                network_protocol = NETWORK_PROTOCOLS_REDIS;
        }

        for(int binding_index = 0; binding_index < config_network_protocol->bindings_count; binding_index++) {
            uint8_t listeners_count_before = listener_new_cb_user_data.listeners_count;
            if (network_channel_listener_new(
                    config_network_protocol->bindings[binding_index].host,
                    config_network_protocol->bindings[binding_index].port,
                    config_network->listen_backlog,
                    network_protocol,
                    &listener_new_cb_user_data) == false) {

                LOG_E(TAG, "Unable to setup listener for <%s:%u> with protocol <%d>",
                      config_network_protocol->bindings[binding_index].host,
                      config_network_protocol->bindings[binding_index].port,
                      network_protocol);
            }

            for(
                    uint8_t listener_index = listeners_count_before;
                    listener_index < listener_new_cb_user_data.listeners_count;
                    listener_index++) {
                network_channel_t *channel_listener =
                        ((void*)listener_new_cb_user_data.listeners) +
                        (listener_new_cb_user_data.network_channel_size * listener_index);
                channel_listener->protocol_config = config_network_protocol;
            }
        }
    }

    *listeners = listener_new_cb_user_data.listeners;
    *listeners_count = listener_new_cb_user_data.listeners_count;
}

void worker_network_listeners_listen(
        fiber_t **listeners_fibers,
        network_channel_t *listeners,
        uint8_t listeners_count) {
    for (int listener_index = 0; listener_index < listeners_count; listener_index++) {
        network_channel_t *listener_channel = worker_op_network_channel_multi_get(
                listeners,
                listener_index);

        listeners_fibers[listener_index] = fiber_scheduler_new_fiber(
                "worker-listener",
                sizeof("worker-listener") - 1,
                worker_network_listeners_fiber_entrypoint,
                (void *)listener_channel);
    }
}

void worker_network_new_client_fiber_entrypoint(
        void *user_data) {
    worker_stats_t *stats = worker_stats_get();

    network_channel_t *new_channel = user_data;

    stats->network.total.active_connections++;
    stats->network.total.accepted_connections++;
    stats->network.per_second.accepted_connections++;

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
            network_protocol_redis_accept(
                    new_channel);
            break;
    }

    // Close the connection
    if (new_channel->status != NETWORK_CHANNEL_STATUS_CLOSED) {
        worker_op_network_close(new_channel, true);
    }

    // Updates the worker stats
    stats->network.total.active_connections--;

    fiber_scheduler_terminate_current_fiber();
}

void worker_network_listeners_fiber_entrypoint(
        void* user_data) {
    worker_context_t *worker_context = worker_context_get();
    network_channel_t *new_channel = NULL;
    network_channel_t *listener_channel = user_data;

    while(!worker_should_terminate(worker_context)) {
        if ((new_channel = worker_op_network_accept(
                listener_channel)) == NULL) {
            LOG_W(
                    TAG,
                    "[FD:%5d][ACCEPT] Listener <%s> failed to accept a new connection",
                    listener_channel->fd,
                    listener_channel->address.str);

            continue;
        }

        new_channel->status = NETWORK_CHANNEL_STATUS_CONNECTED;

        // TODO: should implement a wrapper for this
        new_channel->timeout.read_ns = new_channel->protocol_config->timeout->read_ms > 0
                                        ? new_channel->protocol_config->timeout->read_ms * 1000000
                                        : new_channel->protocol_config->timeout->read_ms;
        new_channel->timeout.write_ns = new_channel->protocol_config->timeout->write_ms > 0
                                        ? new_channel->protocol_config->timeout->write_ms * 1000000
                                        : new_channel->protocol_config->timeout->write_ms;

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

    // Switch back to the scheduler, as the lister has been closed this fiber will never be invoked and will get freed
    fiber_scheduler_switch_back();
}
