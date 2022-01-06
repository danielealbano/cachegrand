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
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "log/log.h"
#include "fiber.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "config.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "worker/worker_common.h"
#include "fiber_scheduler.h"
#include "worker/worker_op.h"
#include "worker/worker.h"
#include "worker/network/worker_network_op.h"

#include "worker_network.h"

#define TAG "worker_network"

void worker_network_listeners_initialize(
        worker_context_t *worker_context) {
    network_channel_listener_new_callback_user_data_t listener_new_cb_user_data = {0};
    config_t *config = worker_context->config;

    listener_new_cb_user_data.core_index = worker_context->core_index;

    // With listeners = NULL, the number of needed listeners will be enumerated and listeners_count
    // increased as needed
    listener_new_cb_user_data.listeners = NULL;
    for(int protocol_index = 0; protocol_index < config->network->protocols_count; protocol_index++) {
        config_network_protocol_t *config_network_protocol = &config->network->protocols[protocol_index];
        for(int binding_index = 0; binding_index < config_network_protocol->bindings_count; binding_index++) {
            if (network_channel_listener_new(
                    config_network_protocol->bindings[binding_index].host,
                    config_network_protocol->bindings[binding_index].port,
                    worker_context->config->network->listen_backlog,
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
    for(int protocol_index = 0; protocol_index < config->network->protocols_count; protocol_index++) {
        network_protocols_t network_protocol;

        config_network_protocol_t *config_network_protocol = &config->network->protocols[protocol_index];
        switch(config_network_protocol->type) {
            default:
            case CONFIG_PROTOCOL_TYPE_REDIS:
                network_protocol = NETWORK_PROTOCOLS_REDIS;
        }

        for(int binding_index = 0; binding_index < config_network_protocol->bindings_count; binding_index++) {
            if (network_channel_listener_new(
                    config_network_protocol->bindings[binding_index].host,
                    config_network_protocol->bindings[binding_index].port,
                    worker_context->config->network->listen_backlog,
                    network_protocol,
                    &listener_new_cb_user_data) == false) {

                LOG_E(TAG, "Unable to setup listener for <%s:%u> with protocol <%d>",
                      config_network_protocol->bindings[binding_index].host,
                      config_network_protocol->bindings[binding_index].port,
                      network_protocol);
            }
        }
    }

    // Assign the listeners and listeners_count to the worker user data
    worker_context->network.listeners_count = listener_new_cb_user_data.listeners_count;
    worker_context->network.listeners = listener_new_cb_user_data.listeners;
    worker_context->network.network_channel_size = listener_new_cb_user_data.network_channel_size;
}

void worker_network_listeners_listen(
        worker_context_t *worker_context) {
    for (int listener_index = 0; listener_index < worker_context->network.listeners_count; listener_index++) {
        network_channel_t *listener_channel = worker_op_network_channel_multi_get(
                worker_context->network.listeners,
                listener_index);

        fiber_scheduler_new_fiber(
                "worker-listener",
                sizeof("worker-listener") - 1,
                worker_network_listeners_fiber_entrypoint,
                (void *)listener_channel);
    }
}

bool worker_network_buffer_has_enough_space(
        network_channel_buffer_t *read_buffer,
        size_t read_length) {
    size_t read_buffer_needed_size_min = read_buffer->data_size + read_length;

    return read_buffer->length >= read_buffer_needed_size_min;
}

bool worker_network_buffer_needs_rewind(
        network_channel_buffer_t *read_buffer,
        size_t read_length) {
    size_t read_buffer_needed_size_min = read_buffer->data_size + read_length;
    size_t read_buffer_needed_size = read_buffer->data_offset + read_buffer_needed_size_min;

    return read_buffer->length >= read_buffer_needed_size;
}

void worker_network_buffer_rewind(
        network_channel_buffer_t *read_buffer) {
    memcpy(
            read_buffer->data,
            read_buffer->data +
            read_buffer->data_offset,
            read_buffer->data_size);
    read_buffer->data_offset = 0;
}

network_op_result_t worker_network_receive(
        network_channel_t *channel,
        network_channel_buffer_t *read_buffer,
        size_t read_length) {
    size_t buffer_offset =
            read_buffer->data_offset +
            read_buffer->data_size;

    network_channel_buffer_data_t *buffer =
            read_buffer->data +
            buffer_offset;
    size_t buffer_length =
            read_buffer->length -
            buffer_offset;

    if (buffer_length < read_length) {
        LOG_D(
                TAG,
                "Need <%lu> bytes in the buffer but only <%lu> are available for <%s>, too much data, closing connection",
                read_length,
                buffer_length,
                channel->address.str);

        fiber_scheduler_set_error(ENOMEM);
        return NETWORK_OP_RESULT_ERROR;
    }

    int32_t receive_length = worker_op_network_receive(
            channel,
            buffer,
            buffer_length);

    if (receive_length == 0) {
        LOG_D(
                TAG,
                "[FD:%5d][RECV] The client <%s> closed the connection",
                channel->fd,
                channel->address.str);

        return NETWORK_OP_RESULT_CLOSE_SOCKET;
    } else if (receive_length < 0) {
        int error_number = -receive_length;
        LOG_I(
                TAG,
                "[FD:%5d][ERROR CLIENT] Error <%s (%d)> from client <%s>",
                channel->fd,
                strerror(error_number),
                error_number,
                channel->address.str);

        return NETWORK_OP_RESULT_ERROR;
    }

    // Increase the amount of actual data (data_size) in the buffer
    read_buffer->data_size += receive_length;

    LOG_D(
            TAG,
            "[FD:%5d][RECV] Received <%u> bytes from client <%s>",
            channel->fd,
            receive_length,
            channel->address.str);

    worker_context_t *worker_context = worker_context_get();
    worker_context->stats.internal.network.received_packets_per_second++;
    worker_context->stats.internal.network.received_packets_total++;


    return NETWORK_OP_RESULT_OK;
}

network_op_result_t worker_network_send(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length) {
    int32_t send_length = worker_op_network_send(
            channel,
            buffer,
            buffer_length);

    if (send_length == 0) {
        LOG_D(
                TAG,
                "[FD:%5d][SEND] The client <%s> closed the connection",
                channel->fd,
                channel->address.str);

        return NETWORK_OP_RESULT_CLOSE_SOCKET;
    } else if (send_length < 0) {
        int error_number = -send_length;
        LOG_I(
                TAG,
                "[FD:%5d][ERROR CLIENT] Error <%s (%d)> from client <%s>",
                channel->fd,
                strerror(error_number),
                error_number,
                channel->address.str);

        return NETWORK_OP_RESULT_ERROR;
    }

    LOG_D(
            TAG,
            "[FD:%5d][SEND] Sent <%u> bytes to client <%s>",
            channel->fd,
            send_length,
            channel->address.str);

    worker_context_t *context = worker_context_get();
    context->stats.internal.network.sent_packets_per_second++;
    context->stats.internal.network.sent_packets_total++;

    return NETWORK_OP_RESULT_OK;
}

network_op_result_t worker_network_close(
        network_channel_t *channel,
        bool shutdown_may_fail) {
    return worker_op_network_close(channel, shutdown_may_fail)
        ? NETWORK_OP_RESULT_OK
        : NETWORK_OP_RESULT_ERROR;
}
