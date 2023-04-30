/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
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
#include <linux/tls.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "log/log.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "fiber/fiber.h"
#include "config.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "support/io_uring/io_uring_support.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "network/io/network_io_common_tls.h"
#include "network/channel/network_channel_tls.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"
#include "fiber/fiber_scheduler.h"
#include "worker/worker_iouring_op.h"
#include "worker/worker_iouring.h"
#include "worker/network/worker_network_op.h"

#include "worker_network_iouring_op.h"

#define TAG "worker_network_op"

void worker_network_iouring_op_network_post_close(
        network_channel_iouring_t *channel) {
    if (likely(channel->has_mapped_fd)) {
        worker_iouring_fds_map_remove(channel->wrapped_channel.fd);
        channel->wrapped_channel.fd = channel->fd;
        channel->has_mapped_fd = false;
    }

    network_channel_iouring_free(channel);
}

network_channel_t* worker_network_iouring_op_network_accept_setup_new_channel(
        worker_iouring_context_t *context,
        network_channel_iouring_t *listener_channel,
        struct sockaddr *addr,
        socklen_t addr_len,
        io_uring_cqe_t *cqe) {
    worker_context_t *worker_context = worker_context_get();
    worker_stats_t *stats = worker_stats_get_internal_current();

    // Setup the new channel
    network_channel_iouring_t* new_channel = network_channel_iouring_new(NETWORK_CHANNEL_TYPE_CLIENT);
    memcpy(&new_channel->wrapped_channel.address.socket.base, addr, addr_len);
    new_channel->wrapped_channel.address.size = addr_len;
    new_channel->wrapped_channel.module_id = listener_channel->wrapped_channel.module_id;
    new_channel->wrapped_channel.module_config = listener_channel->wrapped_channel.module_config;
    new_channel->fd = new_channel->wrapped_channel.fd = cqe->res;

    // Convert the socket address in a string
    network_io_common_socket_address_str(
            &new_channel->wrapped_channel.address.socket.base,
            new_channel->wrapped_channel.address.str,
            sizeof(new_channel->wrapped_channel.address.str));

    if (unlikely(stats->network.total.active_connections >= worker_context->config->network->max_clients)) {
        LOG_V(
                TAG,
                "[FD:%5d][ACCEPT] Maximum active connections established, can't accept any new connection",
                new_channel->fd);
        worker_network_iouring_op_network_close(
                (network_channel_t *)new_channel,
                true);

        return NULL;
    }

    // Perform the initial setup on the new channel
    if (unlikely(network_channel_client_setup(
            new_channel->wrapped_channel.fd,
            context->core_index) == false)) {
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

    if (listener_channel->wrapped_channel.module_config->network->keepalive != NULL) {
        bool error = false;
        error |= !network_io_common_socket_enable_keepalive(new_channel->wrapped_channel.fd, true);
        error |= !network_io_common_socket_set_keepalive_count(
                new_channel->wrapped_channel.fd,
                listener_channel->wrapped_channel.module_config->network->keepalive->probes);
        error |= !network_io_common_socket_set_keepalive_idle(
                new_channel->wrapped_channel.fd,
                listener_channel->wrapped_channel.module_config->network->keepalive->time);
        error |= !network_io_common_socket_set_keepalive_interval(
                new_channel->wrapped_channel.fd,
                listener_channel->wrapped_channel.module_config->network->keepalive->interval);

        if (error) {
            LOG_W(
                    TAG,
                    "Failed to enable the keepalive settings the connection <%s> coming from listener <%s>",
                    new_channel->wrapped_channel.address.str,
                    listener_channel->wrapped_channel.address.str);
        }
    }

    if (listener_channel->wrapped_channel.tls.enabled) {
        network_channel_tls_set_config(
                &new_channel->wrapped_channel,
                listener_channel->wrapped_channel.tls.config);

        if (unlikely(!network_channel_tls_init(
                &new_channel->wrapped_channel))) {
            LOG_W(
                    TAG,
                    "TLS setup failed for the connection <%s>, coming from listener <%s>",
                    new_channel->wrapped_channel.address.str,
                    listener_channel->wrapped_channel.address.str);

            worker_network_iouring_op_network_close(
                    (network_channel_t *)new_channel,
                    true);

            return NULL;
        }

        if (unlikely(!network_channel_tls_handshake(
                &new_channel->wrapped_channel))) {
            LOG_V(
                    TAG,
                    "TLS handshake failed for the connection <%s>, coming from listener <%s>",
                    new_channel->wrapped_channel.address.str,
                    listener_channel->wrapped_channel.address.str);

            worker_network_iouring_op_network_close(
                    &new_channel->wrapped_channel,
                    true);

            return NULL;
        }

        network_channel_tls_set_enabled(
                &new_channel->wrapped_channel,
                true);

        if (network_channel_tls_ktls_supports_mbedtls_cipher_suite(
                &new_channel->wrapped_channel)) {
            LOG_D(
                    TAG,
                    "kTLS supports the cipher, it can be enabled for the connection <%s>, coming from listener <%s>",
                    new_channel->wrapped_channel.address.str,
                    listener_channel->wrapped_channel.address.str);
            if (network_channel_tls_setup_ktls(&new_channel->wrapped_channel)) {
                // Enable kTLS and ensure mbedtls is disabled
                network_channel_tls_set_ktls(
                        &new_channel->wrapped_channel,
                        true);
                network_channel_tls_set_mbedtls(
                        &new_channel->wrapped_channel,
                        false);

                LOG_D(
                        TAG,
                        "kTLS successfully enabled for connection <%s>, coming from listener <%s>",
                        new_channel->wrapped_channel.address.str,
                        listener_channel->wrapped_channel.address.str);
            } else {
                LOG_D(
                        TAG,
                        "Failed to enable kTLS for the connection <%s>, coming from listener <%s>, using mbedtls",
                        new_channel->wrapped_channel.address.str,
                        listener_channel->wrapped_channel.address.str);
            }
        }

        // If kTLS can't be enabled or its activation fails, enable mbedtls
        if (!network_channel_tls_uses_ktls(&new_channel->wrapped_channel)) {
            network_channel_tls_set_mbedtls(
                    &new_channel->wrapped_channel,
                    true);
        }

        // If the client has sent a client certificate, report the common name in the logs
        if (network_channel_tls_has_peer_certificate(&new_channel->wrapped_channel)) {
            const char *cn = NULL;
            size_t cn_length = 0;

            if (network_channel_tls_peer_certificate_get_cn(
                    &new_channel->wrapped_channel,
                    &cn,
                    &cn_length)) {
                LOG_D(
                        TAG,
                        "TLS client certificate common name: %.*s",
                        (int)cn_length,
                        cn);
            }
        }
    }

    if (unlikely(!worker_iouring_fds_map_add_and_enqueue_files_update(
            worker_iouring_context_get()->ring,
            new_channel->fd,
            WORKER_FDS_MAP_FILES_FD_TYPE_NETWORK_CHANNEL,
            &new_channel->has_mapped_fd,
            &new_channel->base_sqe_flags,
            &new_channel->wrapped_channel.fd))) {
        LOG_E(
                TAG,
                "Can't accept the new connection <%s> coming from listener <%s>, unable to find a free fds slot",
                new_channel->wrapped_channel.address.str,
                listener_channel->wrapped_channel.address.str);

        worker_network_iouring_op_network_close(
                (network_channel_t *)new_channel,
                true);

        return NULL;
    }

    return (network_channel_t*)new_channel;
}

network_channel_t* worker_network_iouring_op_network_accept(
        network_channel_t *listener_channel) {
    uint8_t socket_address[128];
    socklen_t socket_address_size = sizeof(socket_address);

    // The memory allocated here will get lost (valgrind will report it) when cachegrand shutdown because the fiber
    // never gets the chance to terminate. This is a wanted behaviour.
    worker_iouring_context_t *context = worker_iouring_context_get();

    fiber_scheduler_reset_error();

    bool res = io_uring_support_sqe_enqueue_accept(
            context->ring,
            listener_channel->fd,
            (struct sockaddr*)&socket_address,
            &socket_address_size,
            0,
            0,
            (uintptr_t) fiber_scheduler_get_current());

    if (unlikely(res == false)) {
        fiber_scheduler_set_error(ENOMEM);
        return NULL;
    }

    // Switch the execution back to the scheduler
    fiber_scheduler_switch_back();

    // When the fiber continues the execution, it has to fetch the return value
    io_uring_cqe_t *cqe = (io_uring_cqe_t*)((fiber_scheduler_get_current())->ret.ptr_value);

    // Validate the result
    if (unlikely(worker_iouring_cqe_is_error_any(cqe))) {
        worker_context_t *worker_context = worker_context_get();
        if (cqe->res == -EINVAL && worker_should_terminate(worker_context)) {
            // The worker is terminating, the error is the consequence of closing the listener socket
            return NULL;
        }

        fiber_scheduler_set_error(-cqe->res);
        LOG_E(
                TAG,
                "Error while accepting a connection on listener <%s>",
                listener_channel->address.str);
        LOG_E_OS_ERROR(TAG);

        return NULL;
    }

    // Setup the new channel
    network_channel_t *new_channel = worker_network_iouring_op_network_accept_setup_new_channel(
            context,
            (network_channel_iouring_t *)listener_channel,
            (struct sockaddr*)&socket_address,
            socket_address_size,
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

    if (unlikely(!res)) {
        fiber_scheduler_set_error(errno);
    }

    worker_network_iouring_op_network_post_close(
            channel_iouring);

    return res;
}

int32_t worker_network_iouring_op_network_receive(
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length) {
    int32_t res;
    worker_iouring_context_t *context = worker_iouring_context_get();
    kernel_timespec_t kernel_timespec = {
            .tv_sec = channel->timeout.read.sec,
            .tv_nsec = channel->timeout.read.nsec,
    };

    fiber_scheduler_reset_error();

    do {
        uint8_t extra_sqes = 0;

        if (kernel_timespec.tv_nsec != -1) {
            extra_sqes |= IOSQE_IO_LINK;
        }

        if (unlikely(!io_uring_support_sqe_enqueue_recv(
                context->ring,
                channel->fd,
                buffer,
                buffer_length,
                0,
                ((network_channel_iouring_t*)channel)->base_sqe_flags | extra_sqes,
                (uintptr_t) fiber_scheduler_get_current()))) {
            fiber_scheduler_set_error(ENOMEM);
            return -ENOMEM;
        }

        if (kernel_timespec.tv_nsec != -1) {
            if (unlikely(!io_uring_support_sqe_enqueue_link_timeout(
                    context->ring,
                    &kernel_timespec,
                    0,
                    0))) {
                fiber_scheduler_set_error(ENOMEM);
                return -ENOMEM;
            }
        }

        // Switch the execution back to the scheduler
        fiber_scheduler_switch_back();

        // When the fiber continues the execution, it has to fetch the return value
        io_uring_cqe_t *cqe = (io_uring_cqe_t*)((fiber_scheduler_get_current())->ret.ptr_value);

        res = cqe->res;
    } while(unlikely(res == -EAGAIN));

    // If kTLS is enabled, EPIPE, EIO or EBADMSG can be returned in case of a connection reset, we don't really want to
    // spam the logs with these messages so res gets set to 0 to "pretend" the connection has been closed by the remote
    // endpoint gracefully
    if (channel->tls.ktls && (res == -EIO || res == -EBADMSG || res == -EPIPE)) {
        res = 0;
    } else if (unlikely(res < 0)) {
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
            .tv_sec = channel->timeout.read.sec,
            .tv_nsec = channel->timeout.read.nsec,
    };

    fiber_scheduler_reset_error();

    do {
        uint8_t extra_sqes = 0;

        if (kernel_timespec.tv_nsec != -1) {
            extra_sqes |= IOSQE_IO_LINK;
        }

        if (unlikely(!io_uring_support_sqe_enqueue_send(
                context->ring,
                channel->fd,
                buffer,
                buffer_length,
                0,
                ((network_channel_iouring_t*)channel)->base_sqe_flags | extra_sqes,
                (uintptr_t) fiber_scheduler_get_current()))) {
            fiber_scheduler_set_error(ENOMEM);
            return -ENOMEM;
        }

        if (kernel_timespec.tv_nsec != -1) {
            if (unlikely(!io_uring_support_sqe_enqueue_link_timeout(
                    context->ring,
                    &kernel_timespec,
                    0,
                    0))) {
                fiber_scheduler_set_error(ENOMEM);
                return -ENOMEM;
            }
        }

        // Switch the execution back to the scheduler
        fiber_scheduler_switch_back();

        // When the fiber continues the execution, it has to fetch the return value
        io_uring_cqe_t *cqe = (io_uring_cqe_t*)((fiber_scheduler_get_current())->ret.ptr_value);

        res = cqe->res;
    } while(unlikely(res == -EAGAIN));

    // If kTLS is enabled, EIO or EBADMSG can be returned in case of a connection reset, we don't really want to spam
    // the logs with these messages so res gets set to 0 to "pretend" the connection has been closed by the remote
    // endpoint gracefully
    if (channel->tls.ktls && (res == -EIO || res == -EBADMSG)) {
        res = 0;
    } else if (unlikely(res < 0)) {
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

network_channel_t* worker_network_iouring_network_channel_new(
        network_channel_type_t type) {
    return (network_channel_t*)network_channel_iouring_new(type);
}

void worker_network_iouring_network_channel_free(
        network_channel_t *channel) {
    network_channel_iouring_free((network_channel_iouring_t*)channel);
}

network_channel_t* worker_network_iouring_network_channel_multi_new(
        network_channel_type_t type,
        uint32_t count) {
    return (network_channel_t*)network_channel_iouring_multi_new(type, count);
}

network_channel_t* worker_network_iouring_network_channel_multi_get(
        network_channel_t *channels,
        uint32_t index) {
    uintptr_t offset = worker_network_iouring_op_network_channel_size() * index;
    uintptr_t channels_ptr = (uintptr_t)channels;
    return (network_channel_t*)(channels_ptr + offset);
}

void worker_network_iouring_network_channel_multi_free(
        network_channel_t *channels,
        uint32_t count) {
    network_channel_iouring_multi_free((network_channel_iouring_t*)channels, count);
}

size_t worker_network_iouring_op_network_channel_size() {
    return sizeof(network_channel_iouring_t);
}

bool worker_network_iouring_op_register() {
    worker_op_network_channel_new = worker_network_iouring_network_channel_new;
    worker_op_network_channel_multi_new = worker_network_iouring_network_channel_multi_new;
    worker_op_network_channel_multi_get = worker_network_iouring_network_channel_multi_get;
    worker_op_network_channel_multi_free = worker_network_iouring_network_channel_multi_free;
    worker_op_network_channel_size = worker_network_iouring_op_network_channel_size;
    worker_op_network_channel_free = worker_network_iouring_network_channel_free;
    worker_op_network_accept = worker_network_iouring_op_network_accept;
    worker_op_network_receive = worker_network_iouring_op_network_receive;
    worker_op_network_send = worker_network_iouring_op_network_send;
    worker_op_network_close = worker_network_iouring_op_network_close;

    return true;
}
