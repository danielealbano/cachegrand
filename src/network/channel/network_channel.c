/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
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
#include "network/protocol/network_protocol.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "network/io/network_io_common.h"
#include "data_structures/hashtable/mcmp/hashtable.h"

#include "network/channel/network_channel.h"

#define TAG "network_channel"

bool network_channel_client_setup(
        network_io_common_fd_t fd,
        uint32_t incoming_cpu) {
    bool error = false;

    error |= !network_io_common_socket_set_incoming_cpu(fd, (int)incoming_cpu);
    error |= !network_io_common_socket_set_quickack(fd, true);
    error |= !network_io_common_socket_set_linger(fd, true, 2);
    error |= !network_io_common_socket_set_keepalive(fd, true);
    error |= !network_io_common_socket_set_receive_timeout(fd, 5, 0);
    error |= !network_io_common_socket_set_send_timeout(fd, 5, 0);

    return !error;
}

bool network_channel_server_setup(
        network_io_common_fd_t fd,
        uint32_t incoming_cpu) {
    bool error = false;

    // TODO: Replace with an eBPF reuseport program, example:
    /**
            // A = raw_smp_processor_id()
            code[0] = new sock_filter { code = BPF_LD | BPF_W | BPF_ABS, k = SKF_AD_OFF + SKF_AD_CPU };
            // return A
            code[1] = new sock_filter { code = BPF_RET | BPF_A };
     */

    error |= !network_io_common_socket_set_incoming_cpu(fd, (int)incoming_cpu);
    error |= !network_io_common_socket_set_reuse_port(fd, true);
    error |= !network_io_common_socket_set_quickack(fd, true);

    return !error;
}

bool network_channel_listener_new_callback_socket_setup_server_cb(
        int fd,
        void *user_data) {
    network_channel_listener_new_callback_user_data_t *cb_user_data = user_data;

    return network_channel_server_setup(fd, cb_user_data->core_index);
}

bool network_channel_init(
        network_channel_t *channel) {
    channel->address.size = sizeof(channel->address.socket);

    return true;
}

bool network_channel_listener_new_callback(
        int family,
        struct sockaddr *socket_address,
        socklen_t socket_address_size,
        uint16_t port,
        uint16_t backlog,
        network_protocols_t protocol,
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
        network_protocols_t protocol,
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
