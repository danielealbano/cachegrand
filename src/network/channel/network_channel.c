/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <assert.h>

#include "exttypes.h"
#include "misc.h"
#include "spinlock.h"
#include "log/log.h"
#include "xalloc.h"
#include "config.h"
#include "modules/module.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "network/io/network_io_common.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "slab_allocator.h"

#include "network/channel/network_channel.h"

#define TAG "network_channel"

bool network_channel_client_setup(
        network_io_common_fd_t fd,
        uint32_t incoming_cpu) {
    bool error = false;

    error |= !network_io_common_socket_set_incoming_cpu(fd, (int)incoming_cpu);
    error |= !network_io_common_socket_set_quickack(fd, true);
    // Because of how cachegrand works (fibers), no reason to keep the nagle algorithm enabled, also in the vast
    // majority of the cases in cachegrand, the data can be sent immediately
    error |= !network_io_common_socket_set_nodelay(fd, true);
    error |= !network_io_common_socket_set_linger(fd, true, 2);
    error |= !network_io_common_socket_set_keepalive(fd, true);
    error |= !network_io_common_socket_set_receive_timeout(fd, 5, 0);
    error |= !network_io_common_socket_set_send_timeout(fd, 5, 0);

    return !error;
}

bool network_channel_server_setup(
        network_io_common_fd_t fd,
        uint32_t incoming_cpu) {
    if (!network_io_common_socket_set_incoming_cpu(fd, (int)incoming_cpu)) {
        LOG_E(TAG, "Failed to set the incoming_cpu for the listener");
        return false;
    }

    if (!network_io_common_socket_set_quickack(fd, true)) {
        LOG_E(TAG, "Failed to set the quickack for the listener");
        return false;
    }

    if (!network_io_common_socket_set_reuse_port(fd, true)) {
        LOG_E(TAG, "Failed to enable the reuse port for the listener");
        return false;
    }

    return true;
}

bool network_channel_listener_new_callback_socket_setup_server_cb(
        int fd,
        void *user_data) {
    network_channel_listener_new_callback_user_data_t *cb_user_data = user_data;

    return network_channel_server_setup(fd, cb_user_data->core_index);
}

bool network_channel_init(
        network_channel_type_t type,
        network_channel_t *channel) {
    channel->type = type;
    channel->address.size = sizeof(channel->address.socket);
    channel->timeout.read_ns = -1;
    channel->timeout.write_ns = -1;

    if (channel->type == NETWORK_CHANNEL_TYPE_CLIENT) {
        channel->buffers.send.length = NETWORK_CHANNEL_SEND_BUFFER_SIZE;
        channel->buffers.send.data = slab_allocator_mem_alloc(channel->buffers.send.length);
    }

    return true;
}

void network_channel_cleanup(
        network_channel_t *channel) {
    if (channel->type == NETWORK_CHANNEL_TYPE_CLIENT) {
        slab_allocator_mem_free(channel->buffers.send.data);
        channel->buffers.send.data = NULL;
    }
}

bool network_channel_listener_new_callback(
        int family,
        struct sockaddr *socket_address,
        socklen_t socket_address_size,
        uint16_t port,
        uint16_t backlog,
        module_types_t protocol,
        void* user_data) {
    int fd;
    network_channel_t* listener;
    network_channel_listener_new_callback_user_data_t *cb_user_data = user_data;

    // If listeners is set to null the callback will do nothing, this process is used only to
    // enumerate the listeners to allocate
    if (cb_user_data->listeners == NULL) {
        cb_user_data->listeners_count++;
        return true;
    }

    // If it's not enumeration ensure that the network_channel_size is greater than 0
    if (cb_user_data->network_channel_size == 0) {
        return false;
    }

    fd = network_io_common_socket_new_server(
            family,
            0,
            socket_address,
            port,
            backlog,
            network_channel_listener_new_callback_socket_setup_server_cb,
            user_data);

    if (fd == -1) {
        return false;
    }

    // TODO: all the networking is implemented in the worker but it's non sense and should be moved to the networking,
    //       because of this nonsense it's not possible use worker_op_network_channel_multi_get here to get the
    //       network_channel and the math is required. Has to be changed once the code is refactored.
    // The size of the struct is dependant on the network backend, listeners can't be accessed as a plain array as the
    // backend may have encapsulated the generic network_channel structure into its own structure and therefore
    // accessing the elements past 0 would actually ending up overriding the network backend own data.
    // The network_channel_size comes into help as the network backend can set it to the appropriate value to let the
    // code properly handle the encapsulation as needed.
    listener =
            ((void*)cb_user_data->listeners) +
            (cb_user_data->network_channel_size * cb_user_data->listeners_count);
    listener->fd = fd;
    listener->address.size = socket_address_size;
    listener->protocol = protocol;
    listener->type = NETWORK_CHANNEL_TYPE_LISTENER;

    memcpy(
            &listener->address.socket.base,
            socket_address,
            socket_address_size);

    network_io_common_socket_address_str(
            socket_address,
            listener->address.str,
            sizeof(listener->address.str));

    LOG_V(TAG, "Created listener for <%s>", listener->address.str);

    cb_user_data->listeners_count++;

    return true;
}

bool network_channel_listener_new(
        char* address,
        uint16_t port,
        uint16_t backlog,
        module_types_t protocol,
        network_channel_listener_new_callback_user_data_t *user_data) {
    int res;

    res = network_io_common_parse_addresses_foreach(
            address,
            port,
            backlog,
            network_channel_listener_new_callback,
            protocol,
            user_data);

    if (res == -1) {
        return false;
    }

    return true;
}
