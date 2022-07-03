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
#include <liburing.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "log/log.h"
#include "spinlock.h"
#include "fiber.h"
#include "config.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "slab_allocator.h"
#include "support/io_uring/io_uring_support.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"
#include "fiber_scheduler.h"
#include "worker/worker_iouring_op.h"
#include "worker/worker_iouring.h"
#include "worker/network/worker_network_op.h"

#include "worker_network_iouring_op.h"

#define TAG "worker_network_op"

void worker_network_iouring_op_network_post_close(
        network_channel_iouring_t *channel) {
    if (channel->has_mapped_fd) {
        worker_iouring_fds_map_remove(channel->wrapped_channel.fd);
        channel->wrapped_channel.fd = channel->fd;
        channel->has_mapped_fd = false;
    }

    network_channel_iouring_free(channel);
}

network_channel_t* worker_network_iouring_op_network_accept_setup_new_channel(
        worker_iouring_context_t *context,
        network_channel_iouring_t *listener_channel,
        network_channel_iouring_t *new_channel,
        io_uring_cqe_t *cqe) {
    if (worker_iouring_cqe_is_error_any(cqe)) {
        fiber_scheduler_set_error(-cqe->res);
        LOG_E(
                TAG,
                "Error while accepting a connection on listener <%s>",
                listener_channel->wrapped_channel.address.str);
        LOG_E_OS_ERROR(TAG);

        return NULL;
    }

    // Setup the new channel
    new_channel->fd = new_channel->wrapped_channel.fd = cqe->res;
    new_channel->wrapped_channel.protocol = listener_channel->wrapped_channel.protocol;
    new_channel->wrapped_channel.protocol_config = listener_channel->wrapped_channel.protocol_config;
    new_channel->wrapped_channel.type = NETWORK_CHANNEL_TYPE_CLIENT;

    // Convert the socket address in a string
    network_io_common_socket_address_str(
            &new_channel->wrapped_channel.address.socket.base,
            new_channel->wrapped_channel.address.str,
            sizeof(new_channel->wrapped_channel.address.str));

    // Perform the initial setup on the new channel
    if (network_channel_client_setup(
            new_channel->wrapped_channel.fd,
            context->core_index) == false) {
        fiber_scheduler_set_error(errno);
        LOG_E(
                TAG,
                "Can't accept the connection <%s> coming from listener <%s>",
                new_channel->wrapped_channel.address.str,
                listener_channel->wrapped_channel.address.str);

        worker_network_iouring_op_network_close(
                (network_channel_t *)new_channel,
                true);

        return NULL;
    }

    if (!worker_iouring_fds_map_add_and_enqueue_files_update(
            worker_iouring_context_get()->ring,
            new_channel->fd,
            &new_channel->has_mapped_fd,
            &new_channel->base_sqe_flags,
            &new_channel->wrapped_channel.fd)) {
        LOG_E(
                TAG,
                "Can't accept the new connection <%s> coming from listener <%s>, unable to find a free fds slot",
                new_channel->wrapped_channel.address.str,
                listener_channel->wrapped_channel.address.str);

        network_io_common_socket_close(new_channel->wrapped_channel.fd, true);
        network_channel_iouring_free(new_channel);

        return NULL;
    }

    return (network_channel_t*)new_channel;
}

network_channel_t* worker_network_iouring_op_network_accept(
        network_channel_t *listener_channel) {
    // The memory allocated here will get lost (valgrind will report it) when cachegrand shutdown because the fiber
    // never gets the chance to terminate. This is a wanted behaviour.
    worker_iouring_context_t *context = worker_iouring_context_get();
    network_channel_iouring_t* new_channel_temp = network_channel_iouring_new();

    fiber_scheduler_reset_error();

    bool res = io_uring_support_sqe_enqueue_accept(
            context->ring,
            listener_channel->fd,
            &new_channel_temp->wrapped_channel.address.socket.base,
            &new_channel_temp->wrapped_channel.address.size,
            0,
            ((network_channel_iouring_t*)listener_channel)->base_sqe_flags,
            (uintptr_t) fiber_scheduler_get_current());

    if (res == false) {
        network_channel_iouring_free(new_channel_temp);
        fiber_scheduler_set_error(ENOMEM);
        return NULL;
    }

    // Switch the execution back to the scheduler
    fiber_scheduler_switch_back();

    // When the fiber continues the execution, it has to fetch the return value
    io_uring_cqe_t *cqe = (io_uring_cqe_t*)((fiber_scheduler_get_current())->ret.ptr_value);

    network_channel_t *new_channel = worker_network_iouring_op_network_accept_setup_new_channel(
            context,
            (network_channel_iouring_t *) listener_channel,
            new_channel_temp,
            cqe);

    return new_channel;
}

bool worker_network_iouring_op_network_close(
        network_channel_t *channel,
        bool shutdown_may_fail) {
    fiber_scheduler_reset_error();

    network_channel_iouring_t *channel_iouring = (network_channel_iouring_t *)channel;
    bool res = network_io_common_socket_close(
            channel_iouring->fd,
            shutdown_may_fail);

    if (!res) {
        fiber_scheduler_set_error(errno);
    }

    worker_network_iouring_op_network_post_close(
            (network_channel_iouring_t *)channel);

    return res;
}

int32_t worker_network_iouring_op_network_receive(
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length) {
    int32_t res;
    worker_iouring_context_t *context = worker_iouring_context_get();
    kernel_timespec_t kernel_timespec = {
            .tv_sec = 0,
            .tv_nsec = channel->timeout.read_ns,
    };

    fiber_scheduler_reset_error();

    do {
        uint8_t extra_sqes = 0;

        if (kernel_timespec.tv_nsec != -1) {
            extra_sqes |= IOSQE_IO_LINK;
        }

        if (!io_uring_support_sqe_enqueue_recv(
                context->ring,
                channel->fd,
                buffer,
                buffer_length,
                0,
                ((network_channel_iouring_t*)channel)->base_sqe_flags | extra_sqes,
                (uintptr_t) fiber_scheduler_get_current())) {
            fiber_scheduler_set_error(ENOMEM);
            return -ENOMEM;
        }

        if (kernel_timespec.tv_nsec != -1) {
            if (!io_uring_support_sqe_enqueue_link_timeout(
                    context->ring,
                    &kernel_timespec,
                    0,
                    0)) {
                fiber_scheduler_set_error(ENOMEM);
                return -ENOMEM;
            }
        }

        // Switch the execution back to the scheduler
        fiber_scheduler_switch_back();

        // When the fiber continues the execution, it has to fetch the return value
        io_uring_cqe_t *cqe = (io_uring_cqe_t*)((fiber_scheduler_get_current())->ret.ptr_value);

        res = cqe->res;
    } while(res == -EAGAIN);

    if (res < 0) {
        fiber_scheduler_set_error(-res);
    }

    return res;
}

int32_t worker_network_iouring_op_network_send(
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length) {
    int32_t res;
    worker_iouring_context_t *context = worker_iouring_context_get();
    kernel_timespec_t kernel_timespec = {
            .tv_sec = 0,
            .tv_nsec = channel->timeout.write_ns,
    };

    fiber_scheduler_reset_error();

    do {
        uint8_t extra_sqes = 0;

        if (kernel_timespec.tv_nsec != -1) {
            extra_sqes |= IOSQE_IO_LINK;
        }

        if (!io_uring_support_sqe_enqueue_send(
                context->ring,
                channel->fd,
                buffer,
                buffer_length,
                0,
                ((network_channel_iouring_t*)channel)->base_sqe_flags | extra_sqes,
                (uintptr_t) fiber_scheduler_get_current())) {
            fiber_scheduler_set_error(ENOMEM);
            return -ENOMEM;
        }

        if (kernel_timespec.tv_nsec != -1) {
            if (!io_uring_support_sqe_enqueue_link_timeout(
                    context->ring,
                    &kernel_timespec,
                    0,
                    0)) {
                fiber_scheduler_set_error(ENOMEM);
                return -ENOMEM;
            }
        }

        // Switch the execution back to the scheduler
        fiber_scheduler_switch_back();

        // When the fiber continues the execution, it has to fetch the return value
        io_uring_cqe_t *cqe = (io_uring_cqe_t*)((fiber_scheduler_get_current())->ret.ptr_value);

        res = cqe->res;
    } while(res == -EAGAIN);

    if (res < 0) {
        fiber_scheduler_set_error(-res);
    }

    return res;
}

bool worker_network_iouring_initialize(
        __attribute__((unused)) worker_context_t *worker_context) {
    return true;
}

void worker_network_iouring_listeners_listen_pre(
        network_channel_t *listeners,
        uint8_t listeners_count) {
    // Update the fd in the network_channel_iouring to match the one used by the underlying channel
    for(int index = 0; index < listeners_count; index++) {
        network_channel_iouring_t *channel =
                (network_channel_iouring_t*)worker_network_iouring_network_channel_multi_get(listeners, index);
        channel->fd = channel->wrapped_channel.fd;
    }
}

bool worker_network_iouring_cleanup(
        __attribute__((unused)) network_channel_t *listeners,
        __attribute__((unused)) uint8_t listeners_count) {
    // do nothing for now
    return true;
}

network_channel_t* worker_network_iouring_network_channel_new() {
    return (network_channel_t*)network_channel_iouring_new();
}

void worker_network_iouring_network_channel_free(
        network_channel_t* channel) {
    network_channel_iouring_free((network_channel_iouring_t*)channel);
}

network_channel_t* worker_network_iouring_network_channel_multi_new(
        uint32_t count) {
    return (network_channel_t*)network_channel_iouring_multi_new(count);
}

network_channel_t* worker_network_iouring_network_channel_multi_get(
        network_channel_t* channels,
        uint32_t index) {
    return ((void*)channels) + (worker_network_iouring_op_network_channel_size() * index);
}

size_t worker_network_iouring_op_network_channel_size() {
    return sizeof(network_channel_iouring_t);
}

bool worker_network_iouring_op_register() {
    worker_op_network_channel_new = worker_network_iouring_network_channel_new;
    worker_op_network_channel_multi_new = worker_network_iouring_network_channel_multi_new;
    worker_op_network_channel_multi_get = worker_network_iouring_network_channel_multi_get;
    worker_op_network_channel_size = worker_network_iouring_op_network_channel_size;
    worker_op_network_channel_free = worker_network_iouring_network_channel_free;
    worker_op_network_accept = worker_network_iouring_op_network_accept;
    worker_op_network_receive = worker_network_iouring_op_network_receive;
    worker_op_network_send = worker_network_iouring_op_network_send;
    worker_op_network_close = worker_network_iouring_op_network_close;

    return true;
}
