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

#include "worker_network_op.h"

#define TAG "worker_network_op"

void worker_network_post_network_channel_close(
        worker_context_t *context,
        network_channel_t *channel) {
    context->stats.internal.network.active_connections--;
}

bool worker_network_op_completion_cb_network_error_client(
        network_channel_t *channel,
        int error_number,
        char* error_message,
        void* user_data) {

    LOG_V(
            TAG,
            "Error <%s (%d)> from client <%s>",
            error_message,
            error_number,
            channel->address.str);

    return true;
}

bool worker_network_op_completion_cb_network_error_listener(
        network_channel_t *channel,
        int error_number,
        char* error_message,
        void* user_data) {

    LOG_V(
            TAG,
            "Error <%s (%d)> from listener <%s>",
            error_message,
            error_number,
            channel->address.str);

    return true;
}

bool worker_network_op_completion_cb_network_close(
        network_channel_t *channel,
        void* user_data) {

    LOG_V(
            TAG,
            "Connection closed for client <%s>",
            channel->address.str);

    worker_network_post_network_channel_close(
            worker_context_get(),
            channel);

    return true;
}

bool worker_network_op_completion_cb_network_receive(
        network_channel_t *channel,
        size_t receive_length,
        void* user_data) {
    worker_context_t *context = worker_context_get();
    worker_network_channel_user_data_t *worker_network_channel_user_data =
            (worker_network_channel_user_data_t*)channel->user_data;

    LOG_V(
            TAG,
            "Received <%d> bytes from client <%s>",
            receive_length,
            channel->address.str);

    context->stats.internal.network.received_packets_per_second++;
    context->stats.internal.network.received_packets_total++;

    // Increase the amount of actual data (data_size) in the buffer
    worker_network_channel_user_data->recv_buffer.data_size += receive_length;

    // TODO: do something with the data to process
    int processed_data_length = 0;

    // Update the buffer offset and size after having processed the data
    worker_network_channel_user_data->recv_buffer.data_offset += processed_data_length;
    worker_network_channel_user_data->recv_buffer.data_size -= processed_data_length;

    char* send_buffer = slab_allocator_mem_alloc(receive_length);
    memcpy(
            send_buffer,
            worker_network_channel_user_data->recv_buffer.data +
                    worker_network_channel_user_data->recv_buffer.data_size -
                    receive_length,
            receive_length);
    return worker_network_send(
            channel,
            send_buffer,
            receive_length,
            NULL);

//    switch (iouring_userdata_current->channel->protocol) {
//        case NETWORK_PROTOCOLS_REDIS:
//            protocol_redis_reader_context_free(
//                    ((network_protocol_redis_context_t*)iouring_userdata_current->channel->user_data.protocol.context)->reader_context);
//            slab_allocator_mem_free(iouring_userdata_current->channel->user_data.protocol.context);
//            iouring_userdata_current->channel->user_data.protocol.context = NULL;
//            break;
//    }
}

bool worker_network_op_completion_cb_network_send(
        network_channel_t *channel,
        size_t send_length,
        void* user_data) {
    worker_context_t *context = worker_context_get();
    worker_network_channel_user_data_t *worker_network_channel_user_data =
            (worker_network_channel_user_data_t *) channel->user_data;

    LOG_V(
            TAG,
            "Sent <%d> bytes to client <%s>",
            send_length,
            channel->address.str);

    context->stats.internal.network.sent_packets_per_second++;
    context->stats.internal.network.sent_packets_total++;

    return worker_network_receive(
            channel,
            NULL);
}

bool worker_network_op_completion_cb_network_accept(
        network_channel_t *listener_channel,
        network_channel_t *new_channel,
        void *user_data) {
    worker_context_t *context = worker_context_get();
    worker_stats_t *stats = &context->stats.internal;

//    switch (iouring_userdata_new->channel->protocol) {
//        default:
//            LOG_E(
//                    TAG,
//                    "Unsupported protocol type <%d>",
//                    iouring_userdata_new->channel->protocol);
//            network_channel_iouring_entry_user_data_free(iouring_userdata_new);
//            break;
//
//        case NETWORK_PROTOCOLS_REDIS:
//            iouring_userdata_new->channel->user_data.protocol.context = slab_allocator_mem_alloc(sizeof(network_protocol_redis_context_t));
//            iouring_userdata_new->channel->user_data.protocol.context = slab_allocator_mem_alloc(sizeof(network_protocol_redis_context_t));
//
//            network_protocol_redis_context_t *context = iouring_userdata_new->channel->user_data.protocol.context;
//            context->reader_context = protocol_redis_reader_context_init();
//
//            iouring_userdata_new->channel->user_data.protocol.context = context;
//            break;
//    }

    if (new_channel == NULL) {
        LOG_W(
                TAG,
                "Failed to accept a new connection coming from listener <%s>",
                listener_channel->address.str);
    } else {
        LOG_V(
                TAG,
                "Listener <%s> accepting new connection from <%s>",
                listener_channel->address.str,
                new_channel->address.str);

        // Setup the network channel user data
        worker_network_channel_user_data_t *worker_network_channel_user_data =
                slab_allocator_mem_alloc(sizeof(worker_network_channel_user_data_t));
        worker_network_channel_user_data->hashtable = context->hashtable;

        // TODO: should get these information from the config file not from a set of defines
        worker_network_channel_user_data->packet_size = NETWORK_CHANNEL_PACKET_SIZE;
        worker_network_channel_user_data->recv_buffer.data = (char *)slab_allocator_mem_alloc(NETWORK_CHANNEL_RECV_BUFFER_SIZE);
        worker_network_channel_user_data->recv_buffer.length = NETWORK_CHANNEL_RECV_BUFFER_SIZE;

        new_channel->user_data = worker_network_channel_user_data;

        // Updates the worker stats
        stats->network.active_connections++;
        stats->network.accepted_connections_total++;
        stats->network.accepted_connections_per_second++;

        // New buffer, no need of any fancy calculation on buffer & buffer_length
        if (worker_network_receive(
                new_channel,
                NULL) == false) {
            LOG_V(
                    TAG,
                    "Unable to start to receive from <%s>, closing connection",
                    new_channel->address.str);
            worker_op_network_close(
                    worker_network_op_completion_cb_network_close,
                    worker_network_op_completion_cb_network_error_client,
                    new_channel,
                    NULL);
        }
    }

    return worker_op_network_accept(
            worker_network_op_completion_cb_network_accept,
            worker_network_op_completion_cb_network_error_listener,
            listener_channel,
            NULL);
}

