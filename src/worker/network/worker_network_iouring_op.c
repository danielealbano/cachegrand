/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <liburing.h>

#include "exttypes.h"
#include "log/log.h"
#include "spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "support/io_uring/io_uring_support.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "config.h"
#include "worker/worker_common.h"
#include "worker/worker.h"
#include "worker/worker_op.h"
#include "worker/worker_iouring_op.h"
#include "worker/worker_iouring.h"
#include "worker/network/worker_network_iouring_op.h"

#define TAG "worker_network_op"

bool worker_iouring_op_network_accept_completion_cb(
        worker_iouring_context_t *context,
        worker_iouring_op_context_t *op_context,
        io_uring_cqe_t *cqe,
        bool *free_op_context) {
    worker_context_t *worker_context;
    network_channel_iouring_t *new_channel, *listener_channel;

    worker_context = context->worker_context;
    listener_channel = op_context->io_uring.network_accept.listener_channel;
    new_channel = op_context->io_uring.network_accept.new_channel;

    if (worker_iouring_cqe_is_error_any(cqe)) {
        LOG_E(
                TAG,
                "Error while waiting for connections on listener <%s>",
                listener_channel->wrapped_channel.address.str);
        worker_request_terminate(worker_context);

        return false;
    }

    // Setup the new channel
    new_channel->fd = new_channel->wrapped_channel.fd = cqe->res;
    new_channel->wrapped_channel.protocol = listener_channel->wrapped_channel.protocol;
    new_channel->wrapped_channel.type = NETWORK_CHANNEL_TYPE_CLIENT;

    // Convert the socket address in a string
    network_io_common_socket_address_str(
            &new_channel->wrapped_channel.address.socket.base,
            new_channel->wrapped_channel.address.str,
            sizeof(new_channel->wrapped_channel.address.str));

    // Perform the initial setup on the new channel
    if (network_channel_client_setup(
            new_channel->wrapped_channel.fd,
            worker_context->core_index) == false) {
        LOG_E(
                TAG,
                "Can't accept the connection <%s> coming from listener <%s>",
                new_channel->wrapped_channel.address.str,
                listener_channel->wrapped_channel.address.str);

        network_io_common_socket_close(new_channel->wrapped_channel.fd, true);
        network_channel_iouring_free(new_channel);
        new_channel = NULL;
    }

    // Map the fd to the iouring registered files shared memory
    if ((worker_iouring_fds_map_add_and_enqueue_files_update(
            worker_iouring_context_get()->ring,
            new_channel)) < 0) {
        LOG_E(
                TAG,
                "Can't accept the new connection <%s> coming from listener <%s>, unable to find a free fds slot",
                new_channel->wrapped_channel.address.str,
                listener_channel->wrapped_channel.address.str);

        network_io_common_socket_close(new_channel->wrapped_channel.fd, true);
        network_channel_iouring_free(new_channel);
        new_channel = NULL;
    }

//    // The SQE to recv will be linked to the previous fd map update
//    if (!worker_iouring_ring_do_send_or_receive(ring, iouring_userdata_new, false)) {
//        LOG_E(
//                TAG,
//                "Can't start to read data from the connection <%s> coming from listener <%s>",
//                iouring_userdata_new->channel->address.str,
//                iouring_userdata_current->listener_new_socket_address.str);
//        network_io_common_socket_close(worker_iouring_fds_map_get(iouring_userdata_new->mapped_fd), true);
//        worker_iouring_fds_map_remove(
//                ring,
//                iouring_userdata_new->mapped_fd,
//                iouring_userdata_new->channel->fd);
//
//        network_channel_iouring_entry_user_data_free(iouring_userdata_current);
//
//        return false;
//    }

    op_context->user.completion_cb.network_accept(
            (network_channel_t*)listener_channel,
            (network_channel_t*)new_channel,
            op_context->user.data);

    return true;
}

bool worker_network_iouring_op_network_accept(
        worker_iouring_context_t *context,
        worker_op_network_accept_completion_cb_fp_t* accept_completion_cb,
        worker_op_network_error_completion_cb_fp_t* error_completion_cb,
        network_channel_t *listener_channel,
        void* user_data) {
    worker_iouring_op_context_t *op_context = worker_iouring_op_context_init(
            worker_iouring_op_network_accept_completion_cb);
    op_context->user.completion_cb.network_accept = accept_completion_cb;
    op_context->user.completion_cb.network_error = error_completion_cb;
    op_context->user.data = user_data;
    op_context->io_uring.network_accept.listener_channel =
            (network_channel_iouring_t*)listener_channel;
    op_context->io_uring.network_accept.new_channel = network_channel_iouring_new();

    return io_uring_support_sqe_enqueue_accept(
            context->ring,
            op_context->io_uring.network_accept.listener_channel->fd,
            &op_context->io_uring.network_accept.new_channel->wrapped_channel.address.socket.base,
            &op_context->io_uring.network_accept.new_channel->wrapped_channel.address.size,
            0,
            op_context->io_uring.network_accept.listener_channel->base_sqe_flags,
            (uintptr_t)op_context);
}

bool worker_network_iouring_op_network_accept_wrapper(
        worker_op_network_accept_completion_cb_fp_t* accept_completion_cb,
        worker_op_network_error_completion_cb_fp_t* error_completion_cb,
        network_channel_t *listener_channel,
        void* user_data) {
    return worker_network_iouring_op_network_accept(
            worker_iouring_context_get(),
            accept_completion_cb,
            error_completion_cb,
            listener_channel,
            user_data);
}

bool worker_network_iouring_op_network_close_completion_cb(
        worker_iouring_context_t *context,
        worker_iouring_op_context_t *op_context,
        io_uring_cqe_t *cqe,
        bool* free_op_context) {
    bool ret = false;
    network_channel_iouring_t *channel;

    channel = op_context->io_uring.network_close.channel;

    if (worker_iouring_cqe_is_error_any(cqe)) {
        LOG_E(
                TAG,
                "Error while trying to close the connection from <%s>",
                channel->wrapped_channel.address.str);
    }

    ret = op_context->user.completion_cb.network_close(
            (network_channel_t*)channel,
            op_context->user.data);

    network_channel_iouring_free(channel);

    return ret;
}

bool worker_network_iouring_op_network_close(
        worker_iouring_context_t *context,
        worker_op_network_close_completion_cb_fp_t* close_completion_cb,
        worker_op_network_error_completion_cb_fp_t* error_completion_cb,
        network_channel_t *channel,
        void* user_data) {
    worker_iouring_op_context_t *op_context = worker_iouring_op_context_init(
            worker_network_iouring_op_network_close_completion_cb);
    op_context->user.completion_cb.network_close = close_completion_cb;
    op_context->user.completion_cb.network_error = error_completion_cb;
    op_context->user.data = user_data;
    op_context->io_uring.network_close.channel = (network_channel_iouring_t*)channel;

    // Do not try to use registered files, the IORING_OP_CLOSE doesn't support IOSQE_FIXED_FILE
    // as in io_close_prep in fs/io_uring.c there is a specific check to return -EBADFD if used
    return io_uring_support_sqe_enqueue_close(
            context->ring,
            channel->fd,
            0,
            (uintptr_t)op_context);
}

bool worker_network_iouring_op_network_close_wrapper(
        worker_op_network_close_completion_cb_fp_t* close_completion_cb,
        worker_op_network_error_completion_cb_fp_t* error_completion_cb,
        network_channel_t *channel,
        void* user_data) {
    return worker_network_iouring_op_network_close(
            worker_iouring_context_get(),
            close_completion_cb,
            error_completion_cb,
            channel,
            user_data);
}

bool worker_iouring_op_network_receive_completion_cb(
        worker_iouring_context_t *context,
        worker_iouring_op_context_t *op_context,
        io_uring_cqe_t *cqe,
        bool* free_op_context) {
    bool ret = false;
    network_channel_iouring_t *channel;

    channel = op_context->io_uring.network_close.channel;

    if (cqe->res != -EAGAIN && (cqe->res == 0 || worker_iouring_cqe_is_error_any(cqe))) {
        if (cqe->res == 0) {
            ret = op_context->user.completion_cb.network_close(
                    (network_channel_t*)channel,
                    op_context->user.data);
        } else {
            char* error_message = strerror(cqe->res * -1);
            ret = op_context->user.completion_cb.network_error(
                    (network_channel_t*)channel,
                    cqe->res,
                    error_message,
                    op_context->user.data);
            network_io_common_socket_close(
                    channel->wrapped_channel.fd, true);
        }

        worker_iouring_fds_map_remove(
            channel->mapped_fd);

        network_channel_iouring_free(channel);
    } else {
        // In some cases io_uring returns an EAGAIN because one of the code paths submits a
        // non blocking read. If it's the case, simply set the length and let the callback
        // deal with it (basically do nothing and resubmit the read).
        if (cqe->res == -EAGAIN) {
            *free_op_context = false;
            ret = worker_network_iouring_op_network_receive_submit_sqe(
                    context,
                    op_context);
        } else {
            ret = op_context->user.completion_cb.network_receive(
                    (network_channel_t*)channel,
                    cqe->res,
                    op_context->user.data);
        }
    }

    return ret;
}

bool worker_network_iouring_op_network_receive_submit_sqe(
        worker_iouring_context_t *context,
        worker_iouring_op_context_t *op_context) {
    return io_uring_support_sqe_enqueue_recv(
            context->ring,
            op_context->io_uring.network_receive.channel->fd,
            op_context->io_uring.network_receive.buffer,
            op_context->io_uring.network_receive.buffer_length,
            0,
            op_context->io_uring.network_receive.channel->base_sqe_flags,
            (uintptr_t)op_context);
}

bool worker_network_iouring_op_network_receive(
        worker_iouring_context_t *context,
        worker_op_network_receive_completion_cb_fp_t* receive_completion_cb,
        worker_op_network_close_completion_cb_fp_t* close_completion_cb,
        worker_op_network_error_completion_cb_fp_t* error_completion_cb,
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length,
        void* user_data) {
    worker_iouring_op_context_t *op_context = worker_iouring_op_context_init(
            worker_iouring_op_network_receive_completion_cb);
    op_context->user.completion_cb.network_receive = receive_completion_cb;
    op_context->user.completion_cb.network_close = close_completion_cb;
    op_context->user.completion_cb.network_error = error_completion_cb;
    op_context->user.data = user_data;
    op_context->io_uring.network_receive.channel = (network_channel_iouring_t*)channel;
    op_context->io_uring.network_receive.buffer = buffer;
    op_context->io_uring.network_receive.buffer_length = buffer_length;

    return worker_network_iouring_op_network_receive_submit_sqe(
            context,
            op_context);
}

bool worker_network_iouring_op_network_receive_wrapper(
        worker_op_network_receive_completion_cb_fp_t* receive_completion_cb,
        worker_op_network_close_completion_cb_fp_t* close_completion_cb,
        worker_op_network_error_completion_cb_fp_t* error_completion_cb,
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length,
        void* user_data) {
    return worker_network_iouring_op_network_receive(
            worker_iouring_context_get(),
            receive_completion_cb,
            close_completion_cb,
            error_completion_cb,
            channel,
            buffer,
            buffer_length,
            user_data);
}

bool worker_iouring_op_network_send_completion_cb(
        worker_iouring_context_t *context,
        worker_iouring_op_context_t *op_context,
        io_uring_cqe_t *cqe,
        bool *free_op_context) {
    bool ret = false;
    network_channel_iouring_t *channel;

    channel = op_context->io_uring.network_close.channel;

    if (cqe->res != -EAGAIN && (cqe->res == 0 || worker_iouring_cqe_is_error_any(cqe))) {
        if (cqe->res == 0) {
            ret = op_context->user.completion_cb.network_close(
                    (network_channel_t*)channel,
                    op_context->user.data);
        } else {
            char* error_message = strerror(cqe->res * -1);
            ret = op_context->user.completion_cb.network_error(
                    (network_channel_t*)channel,
                    cqe->res,
                    error_message,
                    op_context->user.data);
            network_io_common_socket_close(
                    channel->wrapped_channel.fd, true);
        }

        worker_iouring_fds_map_remove(
                channel->mapped_fd);

        network_channel_iouring_free(channel);
    } else {
        if (cqe->res == -EAGAIN) {
            *free_op_context = false;
            ret = worker_network_iouring_op_network_send_submit_sqe(
                    context,
                    op_context);
        } else {
            ret = op_context->user.completion_cb.network_send(
                    (network_channel_t*)channel,
                    cqe->res,
                    op_context->user.data);
        }
    }

    return ret;
}

bool worker_network_iouring_op_network_send_submit_sqe(
        worker_iouring_context_t *context,
        worker_iouring_op_context_t *op_context) {
    return io_uring_support_sqe_enqueue_send(
            context->ring,
            op_context->io_uring.network_send.channel->fd,
            op_context->io_uring.network_send.buffer,
            op_context->io_uring.network_send.buffer_length,
            0,
            op_context->io_uring.network_send.channel->base_sqe_flags,
            (uintptr_t)op_context);
}

bool worker_network_iouring_op_network_send(
        worker_iouring_context_t *context,
        worker_op_network_send_completion_cb_fp_t* send_completion_cb,
        worker_op_network_close_completion_cb_fp_t* close_completion_cb,
        worker_op_network_error_completion_cb_fp_t* error_completion_cb,
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length,
        void* user_data) {
    worker_iouring_op_context_t *op_context = worker_iouring_op_context_init(
            worker_iouring_op_network_send_completion_cb);
    op_context->user.completion_cb.network_send = send_completion_cb;
    op_context->user.completion_cb.network_close = close_completion_cb;
    op_context->user.completion_cb.network_error = error_completion_cb;
    op_context->user.data = user_data;
    op_context->io_uring.network_send.channel = (network_channel_iouring_t*)channel;
    op_context->io_uring.network_send.buffer = buffer;
    op_context->io_uring.network_send.buffer_length = buffer_length;

    return worker_network_iouring_op_network_send_submit_sqe(
            context,
            op_context);
}

bool worker_network_iouring_op_network_send_wrapper(
        worker_op_network_send_completion_cb_fp_t* send_completion_cb,
        worker_op_network_close_completion_cb_fp_t* close_completion_cb,
        worker_op_network_error_completion_cb_fp_t* error_completion_cb,
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length,
        void* user_data) {
    return worker_network_iouring_op_network_send(
            worker_iouring_context_get(),
            send_completion_cb,
            close_completion_cb,
            error_completion_cb,
            channel,
            buffer,
            buffer_length,
            user_data);
}


//        size_t data_offset = iouring_userdata_current->channel->user_data.send_buffer.data_offset;
//        size_t data_size = iouring_userdata_current->channel->user_data.send_buffer.data_size;
//        size_t buffer_size = data_size - data_offset;
//
//        LOG_D(
//                TAG,
//                "[FD:%5d] Submit NETWORK_IO_IOURING_OP_SEND data_offset = %lu, data_size = %lu, buffer_size = %lu",
//                iouring_userdata_current->mapped_fd,
//                data_offset,
//                data_size,
//                buffer_size);
//
//        iouring_userdata_current->op = NETWORK_IO_IOURING_OP_SEND;
//
//        res = io_uring_support_sqe_enqueue_send(
//                ring,
//                fd,
//                (iouring_userdata_current->channel->user_data.send_buffer.data + data_offset),
//                buffer_size,
//                0,
//                sqe_flags,
//                (uintptr_t)iouring_userdata_current);




//
//bool worker_iouring_process_op_recv_close_or_error(
//        worker_context_t *worker_context,
//        worker_stats_t* stats,
//        io_uring_t* ring,
//        io_uring_cqe_t *cqe) {
//    network_channel_iouring_entry_user_data_t *iouring_userdata_current;
//    iouring_userdata_current = (network_channel_iouring_entry_user_data_t *)cqe->user_data;
//
//    if (cqe->res == 0) {
//        LOG_V(TAG, "Closing client <%s>", iouring_userdata_current->channel->address.str);
//    } else{
//        LOG_E(
//                TAG,
//                "Error <%s (%d)>, closing client <%s>",
//                strerror(cqe->res * -1),
//                cqe->res,
//                iouring_userdata_current->channel->address.str);
//    }
//
//    switch (iouring_userdata_current->channel->protocol) {
//        default:
//            LOG_E(
//                    TAG,
//                    "Unsupported protocol type <>",
//                    iouring_userdata_current->channel->protocol);
//            break;
//
//        case NETWORK_PROTOCOLS_REDIS:
//            protocol_redis_reader_context_free(
//                    ((network_protocol_redis_context_t*)iouring_userdata_current->channel->user_data.protocol.context)->reader_context);
//            slab_allocator_mem_free(iouring_userdata_current->channel->user_data.protocol.context);
//            iouring_userdata_current->channel->user_data.protocol.context = NULL;
//            break;
//    }
//
//    stats->network.active_connections--;
//    network_io_common_socket_close(worker_iouring_fds_map_get(iouring_userdata_current->mapped_fd), true);
//    worker_iouring_fds_map_remove(
//            ring,
//            iouring_userdata_current->mapped_fd,
//            iouring_userdata_current->channel->fd);
//    network_channel_iouring_entry_user_data_free(iouring_userdata_current);
//
//    return true;
//}
//
//bool worker_iouring_process_op_recv(
//        worker_context_t *worker_context,
//        worker_stats_t* stats,
//        io_uring_t* ring,
//        io_uring_cqe_t *cqe) {
//    bool result = false;
//    network_channel_iouring_entry_user_data_t *iouring_userdata_current;
//    iouring_userdata_current = (network_channel_iouring_entry_user_data_t *)cqe->user_data;
//
//    if (cqe->res <= 0) {
//        return worker_iouring_process_op_recv_close_or_error(worker_context, stats, ring, cqe);
//    }
//
//    iouring_userdata_current->channel->user_data.recv_buffer.data_size += cqe->res;
//
//    stats->network.received_packets_total++;
//    stats->network.received_packets_per_second++;
//    stats->network.max_packet_size =
//            max(stats->network.max_packet_size, cqe->res);
//
//    char* read_buffer_with_offset =
//            (char*)((uintptr_t)iouring_userdata_current->channel->user_data.recv_buffer.data +
//                iouring_userdata_current->channel->user_data.recv_buffer.data_offset);
//
//    LOG_D(
//            TAG,
//            "[FD:%5d][RECV] before processing - iouring_userdata_current->recv_buffer.offset = %lu, iouring_userdata_current->recv_buffer.size = %lu",
//            iouring_userdata_current->mapped_fd,
//            iouring_userdata_current->channel->user_data.recv_buffer.data_offset,
//            iouring_userdata_current->channel->user_data.recv_buffer.data_size);
//
//    switch (iouring_userdata_current->channel->protocol) {
//        default:
//            LOG_E(
//                    TAG,
//                    "Unsupported protocol type <%d>",
//                    iouring_userdata_current->channel->protocol);
//            break;
//
//        case NETWORK_PROTOCOLS_REDIS:
//            result = network_protocol_redis_recv(
//                    &iouring_userdata_current->channel->user_data,
//                    read_buffer_with_offset);
//    }
//
//    LOG_D(
//            TAG,
//            "[FD:%5d][RECV] after processing - iouring_userdata_current->recv_buffer.offset = %lu, iouring_userdata_current->recv_buffer.size = %lu",
//            iouring_userdata_current->mapped_fd,
//            iouring_userdata_current->channel->user_data.recv_buffer.data_offset,
//            iouring_userdata_current->channel->user_data.recv_buffer.data_size);
//
//    // TODO: if offset + size is bigger than threshold, copy the data back to the beginning but first notify the protocol
//    //       layer
//    size_t recv_buffer_needed_size =
//            iouring_userdata_current->channel->user_data.recv_buffer.data_offset +
//            iouring_userdata_current->channel->user_data.recv_buffer.data_size +
//            NETWORK_CHANNEL_PACKET_SIZE;
//
//    if (recv_buffer_needed_size > iouring_userdata_current->channel->user_data.recv_buffer.length) {
//        size_t min_required_size = iouring_userdata_current->channel->user_data.recv_buffer.data_size +
//                NETWORK_CHANNEL_PACKET_SIZE;
//        if (min_required_size > iouring_userdata_current->channel->user_data.recv_buffer.length) {
//            LOG_D(
//                    TAG,
//                    "[FD:%5d][RECV] Too much unprocessed data into the buffer, unable to continue",
//                    iouring_userdata_current->mapped_fd);
//
//            network_io_common_socket_close(worker_iouring_fds_map_get(iouring_userdata_current->mapped_fd), true);
//            worker_iouring_fds_map_remove(
//                    ring,
//                    iouring_userdata_current->mapped_fd,
//                    iouring_userdata_current->channel->fd);
//
//            network_channel_iouring_entry_user_data_free(iouring_userdata_current);
//
//            return true;
//        } else {
//            LOG_D(
//                    TAG,
//                    "[FD:%5d][RECV] Copying data from the end of the window to the beginning",
//                    iouring_userdata_current->mapped_fd);
//            memcpy(
//                    iouring_userdata_current->channel->user_data.recv_buffer.data,
//                    iouring_userdata_current->channel->user_data.recv_buffer.data +
//                        iouring_userdata_current->channel->user_data.recv_buffer.data_offset,
//                    iouring_userdata_current->channel->user_data.recv_buffer.data_size);
//            iouring_userdata_current->channel->user_data.recv_buffer.data_offset = 0;
//        }
//    }
//
//    if (result) {
//        worker_iouring_ring_do_send_or_receive(ring, iouring_userdata_current, true);
//    }
//
//    return result;
//}
//
//bool worker_iouring_process_op_send(
//        worker_context_t *worker_context,
//        worker_stats_t* stats,
//        io_uring_t* ring,
//        io_uring_cqe_t *cqe) {
//    bool close_connection = false;
//    network_channel_iouring_entry_user_data_t *iouring_userdata_current;
//    iouring_userdata_current = (network_channel_iouring_entry_user_data_t *)cqe->user_data;
//
//    // TODO: need to track the amount of data send contained in cqe->res
//    stats->network.sent_packets_total++;
//    stats->network.sent_packets_per_second++;
//
//    // Check if the connection has to be closed because requested or because of an error
//    if (worker_iouring_cqe_is_error_any(cqe)) {
//        worker_iouring_cqe_log(worker_context, cqe);
//        close_connection = true;
//    } else {
//        iouring_userdata_current->channel->user_data.send_buffer.data_offset += cqe->res;
//
//        if (iouring_userdata_current->channel->user_data.send_buffer.data_offset ==
//            iouring_userdata_current->channel->user_data.send_buffer.data_size) {
//            iouring_userdata_current->channel->user_data.send_buffer.data_size = 0;
//            iouring_userdata_current->channel->user_data.send_buffer.data_offset = 0;
//            iouring_userdata_current->channel->user_data.data_to_send_pending = false;
//
//            if (iouring_userdata_current->channel->user_data.close_connection_on_send) {
//                close_connection = true;
//            }
//        }
//    }
//
//    if (close_connection) {
//        // TODO: cleanup this mess, should be centralized and async!
//        network_io_common_socket_close(worker_iouring_fds_map_get(iouring_userdata_current->mapped_fd), true);
//        worker_iouring_fds_map_remove(
//                ring,
//                iouring_userdata_current->mapped_fd,
//                iouring_userdata_current->channel->fd);
//
//        network_channel_iouring_entry_user_data_free(iouring_userdata_current);
//    } else {
//        worker_iouring_ring_do_send_or_receive(ring, iouring_userdata_current, true);
//    }
//
//    return true;
//}

bool worker_network_iouring_initialize(
        worker_context_t *worker_context) {
    return true;
}

void worker_network_iouring_listeners_listen_pre(
        worker_context_t *worker_context) {
    // Update the fd in the network_channel_iouring to match the one used by the underlying channel
    for(int index = 0; index < worker_context->network.listeners_count; index++) {
        network_channel_iouring_t *channel =
                (network_channel_iouring_t*)&worker_context->network.listeners[index];
        channel->fd = channel->wrapped_channel.fd;
    }
}

void worker_network_iouring_cleanup(
        worker_context_t *worker_context) {
    // do nothing for now
}

network_channel_t* worker_network_iouring_network_channel_new() {
    return (network_channel_t*)network_channel_iouring_new();
}

network_channel_t* worker_network_iouring_network_channel_new_multi(
        int count) {
    return (network_channel_t*)network_channel_iouring_new(count);
}

void worker_network_iouring_network_channel_free(network_channel_t* channel) {
    network_channel_iouring_free((network_channel_iouring_t*)channel);
}

void worker_network_iouring_op_register() {
    worker_op_network_channel_new = worker_network_iouring_network_channel_new;
    worker_op_network_channel_new_multi = worker_network_iouring_network_channel_new_multi;
    worker_op_network_channel_free = worker_network_iouring_network_channel_free;
    worker_op_network_accept = worker_network_iouring_op_network_accept_wrapper;
    worker_op_network_receive = worker_network_iouring_op_network_receive_wrapper;
    worker_op_network_send = worker_network_iouring_op_network_send_wrapper;
    worker_op_network_close = worker_network_iouring_op_network_close_wrapper;
}
