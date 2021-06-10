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

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "log/log.h"
#include "fatal.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "config.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "worker/worker_common.h"
#include "worker/worker_op.h"
#include "worker/network/worker_network_op.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "network/protocol/redis/network_protocol_redis.h"

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
            worker_op_network_channel_new_multi(listener_new_cb_user_data.listeners_count);
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
}

void worker_network_listeners_listen(
        worker_context_t *worker_context) {

    for (int listener_index = 0; listener_index < worker_context->network.listeners_count; listener_index++) {
        network_channel_t *listener_channel = &worker_context->network.listeners[listener_index];

        worker_op_network_accept(
                worker_network_op_completion_cb_network_accept,
                worker_network_op_completion_cb_network_error_listener,
                listener_channel,
                NULL);
    }
}

bool worker_network_receive(
        network_channel_t *channel,
        void* user_data) {
    bool res;
    worker_network_channel_user_data_t *worker_network_channel_user_data =
            (worker_network_channel_user_data_t *)channel->user_data;

    size_t read_buffer_needed_size_min =
            worker_network_channel_user_data->read_buffer.data_size +
            worker_network_channel_user_data->packet_size;
    size_t read_buffer_needed_size =
            worker_network_channel_user_data->read_buffer.data_offset +
            read_buffer_needed_size_min;

    if (read_buffer_needed_size_min > worker_network_channel_user_data->read_buffer.length) {
        LOG_D(
                TAG,
                "[FD:%5d][RECV] Too much unprocessed data into the buffer, unable to continue",
                channel->fd);

        return worker_network_close(channel, NULL);
    } else if (read_buffer_needed_size > worker_network_channel_user_data->read_buffer.length) {
        switch (channel->protocol) {
            default:
                // This can't really happen
                FATAL(
                        TAG,
                        "[FD:%5d][RECEIVE] Unknown protocol <%d>",
                        channel->fd,
                        channel->protocol);
                res = false;
                break;

            case NETWORK_PROTOCOLS_REDIS:
                res = network_protocol_redis_read_buffer_rewind(
                        channel,
                        &worker_network_channel_user_data->read_buffer,
                        worker_network_channel_user_data->protocol.context);
                break;
        }

        if (!res) {
            // TODO: failed to prepare to rewind the data, unable to continue
            FATAL(
                    TAG,
                    "[FD:%5d][RECV]Unable to rewind the data",
                    channel->fd);
        }

        LOG_D(
                TAG,
                "[FD:%5d][RECV] Copying data from the end of the window to the beginning",
                channel->fd);
        memcpy(
                worker_network_channel_user_data->read_buffer.data,
                worker_network_channel_user_data->read_buffer.data +
                worker_network_channel_user_data->read_buffer.data_offset,
                worker_network_channel_user_data->read_buffer.data_size);
        worker_network_channel_user_data->read_buffer.data_offset = 0;
    }

    size_t buffer_offset =
            worker_network_channel_user_data->read_buffer.data_offset +
            worker_network_channel_user_data->read_buffer.data_size;

    network_channel_buffer_data_t *buffer =
            worker_network_channel_user_data->read_buffer.data +
            buffer_offset;
    size_t buffer_length =
            worker_network_channel_user_data->read_buffer.length -
            buffer_offset;

    if (buffer_length < worker_network_channel_user_data->packet_size) {
        LOG_D(
                TAG,
                "Needed <%lu> bytes in the buffer but only <%lu> are available for <%s>, closing connection",
                worker_network_channel_user_data->packet_size,
                buffer_length,
                channel->address.str);

        res = worker_op_network_close(
                worker_network_op_completion_cb_network_close,
                worker_network_op_completion_cb_network_error_client,
                channel,
                user_data);
    } else {
        res = worker_op_network_receive(
                worker_network_op_completion_cb_network_receive,
                worker_network_op_completion_cb_network_close,
                worker_network_op_completion_cb_network_error_client,
                channel,
                buffer,
                buffer_length,
                user_data);
    }

    return res;
}

bool worker_network_send(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length,
        void* user_data) {

    return worker_op_network_send(
            worker_network_op_completion_cb_network_send,
            worker_network_op_completion_cb_network_close,
            worker_network_op_completion_cb_network_error_client,
            channel,
            buffer,
            buffer_length,
            user_data);
}

bool worker_network_close(
        network_channel_t *channel,
        void* user_data) {

    return worker_op_network_close(
            worker_network_op_completion_cb_network_close,
            worker_network_op_completion_cb_network_error_client,
            channel,
            user_data);
}

bool worker_network_protocol_process_buffer(
        network_channel_t *channel,
        worker_network_channel_user_data_t *worker_network_channel_user_data) {
    bool result = false;

    switch (channel->protocol) {
        default:
            FATAL(TAG, "Unsupported protocol type <%d>", channel->protocol);
            break;

        case NETWORK_PROTOCOLS_REDIS:
            result = network_protocol_redis_process_events(
                    channel,
                    worker_network_channel_user_data);
    }

    if (result == false) {
        return worker_network_close(channel, NULL);
    }

    return result;
}

void worker_network_close_connection_on_send(
        network_channel_t *channel,
        bool close_connection_on_send) {
    worker_network_channel_user_data_t *worker_network_channel_user_data =
            (worker_network_channel_user_data_t *)channel->user_data;
    worker_network_channel_user_data->close_connection_on_send = close_connection_on_send;
}