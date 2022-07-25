/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "spinlock.h"
#include "log/log.h"
#include "fiber.h"
#include "fiber_scheduler.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "support/simple_file_io.h"
#include "config.h"
#include "module/module.h"
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

#include "network/network_tls_mbedtls.h"
#include "network/network_tls.h"
#include "network/channel/network_channel_tls.h"

#define TAG "network"

bool network_buffer_has_enough_space(
        network_channel_buffer_t *network_channel_buffer,
        size_t read_length) {
    size_t network_channel_buffer_needed_size_min = network_channel_buffer->data_size + read_length;

    return network_channel_buffer->length >= network_channel_buffer_needed_size_min;
}

bool network_buffer_needs_rewind(
        network_channel_buffer_t *network_channel_buffer,
        size_t read_length) {
    size_t network_channel_buffer_needed_size_min = network_channel_buffer->data_size + read_length;
    size_t network_channel_buffer_needed_size =
            network_channel_buffer->data_offset + network_channel_buffer_needed_size_min;

    return network_channel_buffer_needed_size > network_channel_buffer->length;
}

void network_buffer_rewind(
        network_channel_buffer_t *network_channel_buffer) {
    memcpy(
            network_channel_buffer->data,
            network_channel_buffer->data +
            network_channel_buffer->data_offset,
            network_channel_buffer->data_size);
    network_channel_buffer->data_offset = 0;
}

network_op_result_t network_receive(
        network_channel_t *channel,
        network_channel_buffer_t *buffer,
        size_t receive_length) {
    size_t received_length;

    size_t buffer_data_offset = buffer->data_offset + buffer->data_size;
    network_channel_buffer_data_t *buffer_data = buffer->data + buffer_data_offset;
    size_t buffer_data_length = buffer->length - buffer_data_offset;

    if (unlikely(buffer_data_length < receive_length)) {
        LOG_D(
                TAG,
                "Need <%lu> bytes in the buffer but only <%lu> are available for <%s>, too much data, closing connection",
                receive_length,
                buffer_data_length,
                channel->address.str);

        fiber_scheduler_set_error(ENOMEM);
        return NETWORK_OP_RESULT_ERROR;
    }

    if (unlikely(channel->status == NETWORK_CHANNEL_STATUS_CLOSED)) {
        return NETWORK_OP_RESULT_CLOSE_SOCKET;
    }

    network_op_result_t res;
    if (network_channel_tls_uses_mbedtls(channel)) {
        res = (int32_t)network_tls_receive_internal(
                channel,
                buffer_data,
                buffer_data_length,
                &received_length);
    } else {
        res = (int32_t)network_receive_internal(
                channel,
                buffer_data,
                buffer_data_length,
                &received_length);
    }

    if (likely(res == NETWORK_OP_RESULT_OK)) {
        // Increase the amount of actual data (data_size) in the buffer
        buffer->data_size += received_length;

        // Update stats
        worker_stats_t *stats = worker_stats_get();
        stats->network.per_minute.received_packets++;
        stats->network.total.received_packets++;
        stats->network.per_minute.received_data += received_length;
        stats->network.total.received_data += received_length;

        LOG_D(
                TAG,
                "[FD:%5d][RECV] Received <%lu> bytes from client <%s>",
                channel->fd,
                received_length,
                channel->address.str);
    }

    return res;
}

network_op_result_t network_receive_internal(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length,
        size_t *received_length) {
    *received_length = 0;
    if (unlikely(channel->status == NETWORK_CHANNEL_STATUS_CLOSED)) {
        return NETWORK_OP_RESULT_CLOSE_SOCKET;
    }

    int32_t res = (int32_t)worker_op_network_receive(
            channel,
            buffer,
            buffer_length);

    if (unlikely(res == 0)) {
        LOG_D(
                TAG,
                "[FD:%5d][RECV] The client <%s> closed the connection",
                channel->fd,
                channel->address.str);

        return NETWORK_OP_RESULT_CLOSE_SOCKET;
    } else if (unlikely(res == -ECANCELED)) {
        LOG_I(
                TAG,
                "[FD:%5d][ERROR CLIENT] Receive timeout from client <%s>",
                channel->fd,
                channel->address.str);
        return NETWORK_OP_RESULT_ERROR;
    } else if (unlikely(res < 0)) {
        int error_number = -res;
        LOG_I(
                TAG,
                "[FD:%5d][ERROR CLIENT] Error <%s (%d)> from client <%s>",
                channel->fd,
                strerror(error_number),
                error_number,
                channel->address.str);

        return NETWORK_OP_RESULT_ERROR;
    }

    *received_length = res;

    return NETWORK_OP_RESULT_OK;
}

bool network_should_flush_send_buffer(
        network_channel_t *channel) {
    return channel->status == NETWORK_CHANNEL_STATUS_CONNECTED && channel->buffers.send.data_size > 0;
}

network_op_result_t network_flush_send_buffer(
        network_channel_t *channel) {
    network_op_result_t res;

    if (unlikely(channel->buffers.send.data_size == 0)) {
        return NETWORK_OP_RESULT_OK;
    }

    res = network_send_direct_wrapper(
            channel,
            channel->buffers.send.data,
            channel->buffers.send.data_size);

    // Resets data size and offset
    channel->buffers.send.data_size = 0;
    channel->buffers.send.data_offset = 0;

    return res;
}

network_channel_buffer_data_t *network_send_buffer_acquire_slice(
        network_channel_t *channel,
        size_t slice_length) {
    // Ensure that the slice requested can fit into the buffer and that there isn't already a slice acquired
    assert(slice_length <= channel->buffers.send.length);
    assert(channel->buffers.send_slice_acquired_length == 0);

    // Check if there is enough space on the buffer, if not flush it
    if (unlikely(channel->buffers.send.data_size + slice_length > channel->buffers.send.length)) {
        if (unlikely(network_flush_send_buffer(channel) != NETWORK_OP_RESULT_OK)) {
            return NULL;
        }
    }

#if DEBUG == 1
    channel->buffers.send_slice_acquired_length = slice_length;
#endif

    return channel->buffers.send.data + channel->buffers.send.data_offset;
}

void network_send_buffer_release_slice(
        network_channel_t *channel,
        size_t slice_used_length) {
    // Ensure that when the slice is released, the amount of data used is always the same or smaller than the length
    // acquired. Also ensure that there was a slice acquired.
    assert(channel->buffers.send_slice_acquired_length > 0);
    assert(slice_used_length <= channel->buffers.send_slice_acquired_length);

    channel->buffers.send.data_size += slice_used_length;
    channel->buffers.send.data_offset += slice_used_length;

#if DEBUG == 1
    channel->buffers.send_slice_acquired_length = 0;
#endif
}

network_op_result_t network_send_direct(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length) {
    if (network_should_flush_send_buffer(channel)) {
        network_flush_send_buffer(channel);
    }

    return network_send_direct_wrapper(channel, buffer, buffer_length);
}

network_op_result_t network_send_direct_wrapper(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length) {
    size_t sent_length;
    network_op_result_t res;

    if (network_channel_tls_uses_mbedtls(channel)) {
        res = (int32_t) network_tls_send_direct_internal(
                channel,
                buffer,
                buffer_length,
                &sent_length);
    } else {
        res = (int32_t) network_send_direct_internal(
                channel,
                buffer,
                buffer_length,
                &sent_length);
    }

    if (likely(res == NETWORK_OP_RESULT_OK)) {
        worker_stats_t *stats = worker_stats_get();
        stats->network.per_minute.sent_packets++;
        stats->network.total.sent_packets++;
        stats->network.per_minute.sent_data += sent_length;
        stats->network.total.sent_data += sent_length;

        LOG_D(
                TAG,
                "[FD:%5d][SEND] Sent <%lu> bytes to client <%s>",
                channel->fd,
                sent_length,
                channel->address.str);
    }

    return res;
}

network_op_result_t network_send_buffered(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length) {
    assert(channel->buffers.send_slice_acquired_length == 0);

    do {
        size_t buffer_length_can_be_sent = MIN(buffer_length, channel->buffers.send.length);

        // Check if there is enough room in within send buffer, if not flush it
        if (likely(channel->buffers.send.data_size + buffer_length_can_be_sent > channel->buffers.send.length)) {
            network_op_result_t res = network_flush_send_buffer(channel);

            if (unlikely(res != NETWORK_OP_RESULT_OK)) {
                return res;
            }
        }

        // Copy the data to the send buffer and update data size and offset
        memcpy(
                channel->buffers.send.data + channel->buffers.send.data_offset,
                buffer,
                buffer_length_can_be_sent);
        channel->buffers.send.data_size += buffer_length_can_be_sent;
        channel->buffers.send.data_offset += buffer_length_can_be_sent;

        buffer += buffer_length_can_be_sent;
        buffer_length -= buffer_length_can_be_sent;

        // Normally network_send should be used for small sends, large payloads should be sent using network_send_iov
        // when it will be introduced, to avoid useless data copies.
    } while (unlikely(buffer_length > 0));

    return NETWORK_OP_RESULT_OK;
}

network_op_result_t network_send_direct_internal(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length,
        size_t *sent_length) {
    *sent_length = 0;
    int32_t res = (int32_t)worker_op_network_send(
            channel,
            buffer,
            buffer_length);

    if (unlikely(res == 0)) {
        LOG_D(
                TAG,
                "[FD:%5d][SEND] The client <%s> closed the connection",
                channel->fd,
                channel->address.str);

        return NETWORK_OP_RESULT_CLOSE_SOCKET;
    } else if (unlikely(res == -ECANCELED)) {
        LOG_I(
                TAG,
                "[FD:%5d][ERROR CLIENT] Send timeout to client <%s>",
                channel->fd,
                channel->address.str);
        return NETWORK_OP_RESULT_ERROR;
    } else if (unlikely(res < 0)) {
        int error_number = -res;
        LOG_I(
                TAG,
                "[FD:%5d][ERROR CLIENT] Error <%s (%d)> from client <%s>",
                channel->fd,
                strerror(error_number),
                error_number,
                channel->address.str);

        return NETWORK_OP_RESULT_ERROR;
    }

    *sent_length = res;

    return NETWORK_OP_RESULT_OK;
}

network_op_result_t network_close(
        network_channel_t *channel,
        bool shutdown_may_fail) {
    network_op_result_t res;

    if (network_channel_tls_uses_mbedtls(channel)) {
        res = network_tls_close_internal(
                channel,
                shutdown_may_fail);
    } else {
        res = network_close_internal(
                channel,
                shutdown_may_fail);
    }

    return res;
}

network_op_result_t network_close_internal(
        network_channel_t *channel,
        bool shutdown_may_fail) {
    network_op_result_t res = NETWORK_OP_RESULT_OK;

    assert(channel->status != NETWORK_CHANNEL_STATUS_UNDEFINED);

    if (channel->status != NETWORK_CHANNEL_STATUS_CLOSED) {
        res = worker_op_network_close(channel, shutdown_may_fail)
              ? NETWORK_OP_RESULT_OK
              : NETWORK_OP_RESULT_ERROR;
        channel->status = NETWORK_CHANNEL_STATUS_CLOSED;
    }

    return res;
}
