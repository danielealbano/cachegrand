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
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "log/log.h"
#include "fiber.h"
#include "fiber_scheduler.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "config.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "worker/worker_stats.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_context.h"
#include "worker/worker_op.h"
#include "worker/worker.h"
#include "worker/network/worker_network_op.h"

#include "network.h"

#define TAG "network"

bool network_buffer_has_enough_space(
        network_channel_buffer_t *read_buffer,
        size_t read_length) {
    size_t read_buffer_needed_size_min = read_buffer->data_size + read_length;

    return read_buffer->length >= read_buffer_needed_size_min;
}

bool network_buffer_needs_rewind(
        network_channel_buffer_t *read_buffer,
        size_t read_length) {
    size_t read_buffer_needed_size_min = read_buffer->data_size + read_length;
    size_t read_buffer_needed_size = read_buffer->data_offset + read_buffer_needed_size_min;

    return read_buffer->length >= read_buffer_needed_size;
}

void network_buffer_rewind(
        network_channel_buffer_t *read_buffer) {
    memcpy(
            read_buffer->data,
            read_buffer->data +
            read_buffer->data_offset,
            read_buffer->data_size);
    read_buffer->data_offset = 0;
}

network_op_result_t network_receive(
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

    if (channel->status == NETWORK_CHANNEL_STATUS_CLOSED) {
        return NETWORK_OP_RESULT_CLOSE_SOCKET;
    }

    int32_t receive_length = (int32_t)worker_op_network_receive(
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

    worker_stats_t *stats = worker_stats_get();
    stats->network.per_second.received_packets++;
    stats->network.total.received_packets++;

    return NETWORK_OP_RESULT_OK;
}

network_op_result_t network_send(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length) {
    int32_t send_length = (int32_t)worker_op_network_send(
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

    worker_stats_t *stats = worker_stats_get();
    stats->network.per_second.sent_packets++;
    stats->network.total.sent_packets++;

    return NETWORK_OP_RESULT_OK;
}

network_op_result_t network_close(
        network_channel_t *channel,
        bool shutdown_may_fail) {
    bool res = true;

    assert(channel->status != NETWORK_CHANNEL_STATUS_UNDEFINED);

    if (channel->status != NETWORK_CHANNEL_STATUS_CLOSED) {
        res = worker_op_network_close(channel, shutdown_may_fail)
            ? NETWORK_OP_RESULT_OK
            : NETWORK_OP_RESULT_ERROR;
        channel->status = NETWORK_CHANNEL_STATUS_CLOSED;
    }

    return res;
}
