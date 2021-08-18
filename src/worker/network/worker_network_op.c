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
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "worker/worker_common.h"
#include "worker/worker.h"
#include "worker/worker_op.h"
#include "worker/network/worker_network.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "network/protocol/redis/network_protocol_redis.h"

#include "worker_network_op.h"

#define TAG "worker_network_op"

void worker_network_post_network_channel_close(
        worker_context_t *context,
        network_channel_t *channel,
        void* user_data) {
    worker_network_channel_user_data_t *worker_network_channel_user_data =
            (worker_network_channel_user_data_t*)channel->user_data;

    switch (channel->protocol) {
        default:
            // This can't really happen
            FATAL(
                    TAG,
                    "[FD:%5d][CLOSE] Unknown protocol <%d>",
                    channel->fd,
                    channel->protocol);
            break;

        case NETWORK_PROTOCOLS_REDIS:
            network_protocol_redis_close(channel, worker_network_channel_user_data->protocol.context);
            break;
    }

    slab_allocator_mem_free(channel->user_data);
    channel->user_data = NULL;

    context->stats.internal.network.active_connections--;
}

bool worker_network_op_completion_cb_network_error_client(
        network_channel_t *channel,
        int error_number,
        char* error_message,
        void* user_data) {

    LOG_I(
            TAG,
            "[FD:%5d][ERROR CLIENT] Error <%s (%d)> from client <%s>",
            channel->fd,
            error_message,
            error_number,
            channel->address.str);

    worker_network_post_network_channel_close(
            worker_context_get(),
            channel,
            user_data);

    return true;
}

bool worker_network_op_completion_cb_network_error_listener(
        network_channel_t *channel,
        int error_number,
        char* error_message,
        void* user_data) {

    LOG_E(
            TAG,
            "[FD:%5d][ERROR LISTENER] Error <%s (%d)> from listener <%s>",
            channel->fd,
            error_message,
            error_number,
            channel->address.str);

    // TODO: a listener requires more cleanups

    worker_network_post_network_channel_close(
            worker_context_get(),
            channel,
            user_data);

    return true;
}

bool worker_network_op_completion_cb_network_close(
        network_channel_t *channel,
        void* user_data) {

    LOG_V(
            TAG,
            "[FD:%5d][CLOSE] Connection closed for client <%s>",
            channel->fd,
            channel->address.str);

    worker_network_post_network_channel_close(
            worker_context_get(),
            channel,
            user_data);

    return true;
}

bool worker_network_op_completion_cb_network_receive(
        network_channel_t *channel,
        size_t receive_length,
        void* user_data) {
    int processed_data_length;
    bool result = false;

    worker_context_t *context = worker_context_get();
    worker_network_channel_user_data_t *worker_network_channel_user_data =
            (worker_network_channel_user_data_t*)channel->user_data;

    LOG_D(
            TAG,
            "[FD:%5d][RECV] Received <%lu> bytes from client <%s>",
            channel->fd,
            receive_length,
            channel->address.str);

    context->stats.internal.network.received_packets_per_second++;
    context->stats.internal.network.received_packets_total++;

    // Increase the amount of actual data (data_size) in the buffer
    worker_network_channel_user_data->read_buffer.data_size += receive_length;

    return worker_network_protocol_process_buffer(
            channel,
            worker_network_channel_user_data);
}

bool worker_network_op_completion_cb_network_send(
        network_channel_t *channel,
        size_t send_length,
        void* user_data) {
    worker_context_t *context = worker_context_get();
    worker_network_channel_user_data_t *worker_network_channel_user_data =
            (worker_network_channel_user_data_t *)channel->user_data;

    LOG_D(
            TAG,
            "[FD:%5d][SEND] Sent <%lu> bytes to client <%s>",
            channel->fd,
            send_length,
            channel->address.str);

    context->stats.internal.network.sent_packets_per_second++;
    context->stats.internal.network.sent_packets_total++;

    if (worker_network_channel_user_data->close_connection_on_send) {
        return worker_network_close(
                channel,
                user_data);
    }

    return worker_network_protocol_process_buffer(
            channel,
            worker_network_channel_user_data);
}

bool worker_network_op_completion_cb_network_accept(
        network_channel_t *listener_channel,
        network_channel_t *new_channel,
        void *user_data) {
    bool res = false;
    worker_context_t *context = worker_context_get();
    worker_stats_t *stats = &context->stats.internal;

    if (new_channel == NULL) {
        LOG_W(
                TAG,
                "[FD:%5d][ACCEPT] Failed to accept a new connection coming from listener <%s>",
                listener_channel->fd,
                listener_channel->address.str);
    } else {
        LOG_V(
                TAG,
                "[FD:%5d][ACCEPT] Listener <%s> accepting new connection from <%s>",
                listener_channel->fd,
                listener_channel->address.str,
                new_channel->address.str);

        // Setup the network channel user data
        worker_network_channel_user_data_t *worker_network_channel_user_data =
                slab_allocator_mem_alloc_zero(sizeof(worker_network_channel_user_data_t));
        worker_network_channel_user_data->hashtable = context->hashtable;
        worker_network_channel_user_data->packet_size = NETWORK_CHANNEL_PACKET_SIZE;
        worker_network_channel_user_data->read_buffer.data =
                (char *)slab_allocator_mem_alloc_zero(NETWORK_CHANNEL_RECV_BUFFER_SIZE);

        // To speed up the performances the code takes advantage of SIMD operations that are built to operate on
        // specific amount of data, for example AVX/AVX2 in general operate on 256 bit (32 byte) of data at time.
        // Therefore, to avoid implementing ad hoc checks everywhere and at the same time to ensure that the code will
        // never ever read over the boundary of the allocated block of memory, the length of the read buffer will be
        // initialized to the buffer receive size minus 32.
        // TODO: 32 should be defined as magic const somewhere as it's going hard to track where "32" is in use if it
        //          has to be changed
        // TODO: create a test to ensure that the length of the read buffer is taking into account the 32 bytes of
        //       padding
        worker_network_channel_user_data->read_buffer.length = NETWORK_CHANNEL_RECV_BUFFER_SIZE - 32;

        new_channel->user_data = worker_network_channel_user_data;

        // Updates the worker stats
        stats->network.active_connections++;
        stats->network.accepted_connections_total++;
        stats->network.accepted_connections_per_second++;

        switch (listener_channel->protocol) {
            default:
                // This can't really happen
                FATAL(
                        TAG,
                        "[FD:%5d][ACCEPT] Listener unknown protocol <%d>",
                        listener_channel->fd,
                        listener_channel->protocol);
                break;

            case NETWORK_PROTOCOLS_REDIS:
                res = network_protocol_redis_accept(
                        new_channel,
                        &worker_network_channel_user_data->protocol.context);
                break;
        }
    }

    if (!res) {
        LOG_V(
                TAG,
                "[FD:%5d][ACCEPT] Protocol failed to handle accepted connection <%s>, closing connection",
                new_channel->fd,
                new_channel->address.str);
        worker_network_close(
                new_channel,
                NULL);
    }

    return worker_op_network_accept(
            worker_network_op_completion_cb_network_accept,
            worker_network_op_completion_cb_network_error_listener,
            listener_channel,
            NULL);
}
