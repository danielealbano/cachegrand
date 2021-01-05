#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <assert.h>

#include "misc.h"
#include "log.h"
#include "xalloc.h"
#include "network/io/network_io_common.h"

#include "network/channel/network_channel.h"

#define TAG "network_channel"

bool network_channel_client_setup(
        network_io_common_fd_t fd,
        int incoming_cpu) {
    bool error = false;

    if (!error && network_io_common_socket_set_incoming_cpu(fd, incoming_cpu) == false) {
        error = true;
    }

    if (!error && network_io_common_socket_set_quickack(fd, true) == false) {
        error = true;
    }

    if (!error && network_io_common_socket_set_linger(fd, true, 2) == false) {
        error = true;
    }

    if (!error && network_io_common_socket_set_keepalive(fd, true) == false) {
        error = true;
    }

    if (!error && network_io_common_socket_set_receive_timeout(fd, 5, 0) == false) {
        error = true;
    }

    if (!error && network_io_common_socket_set_send_timeout(fd, 5, 0) == false) {
        error = true;
    }

    return !error;
}

bool network_channel_server_setup(
        network_io_common_fd_t fd,
        int incoming_cpu) {
    bool error = false;

    // TODO: Replace with an eBPF reuseport program, example:
    /**
            // A = raw_smp_processor_id()
            code[0] = new sock_filter { code = BPF_LD | BPF_W | BPF_ABS, k = SKF_AD_OFF + SKF_AD_CPU };
            // return A
            code[1] = new sock_filter { code = BPF_RET | BPF_A };
     */

    if (!error && network_io_common_socket_set_incoming_cpu(fd, incoming_cpu) == false) {
        error = true;
    }

    if (!error && network_io_common_socket_set_reuse_port(fd, true) == false) {
        error = true;
    }

    if (!error && network_io_common_socket_set_quickack(fd, true) == false) {
        error = true;
    }

    return !error;
}

bool network_channel_listener_new_callback_socket_setup_server_cb(
        int fd,
        void *user_data) {
    network_channel_listener_new_callback_user_data_t *cb_user_data = user_data;

    return network_channel_server_setup(fd, cb_user_data->core_index);
}

bool network_channel_listener_new_callback(
        int family,
        struct sockaddr *socket_address,
        socklen_t socket_address_size,
        uint16_t socket_address_index,
        void* user_data) {
    int fd;
    network_channel_listener_new_callback_user_data_t *cb_user_data = user_data;

    assert((cb_user_data->listeners_count + socket_address_index) <
           (sizeof(cb_user_data->listeners) / sizeof(cb_user_data->listeners[0])));

    fd = network_io_common_socket_new_server(
            family,
            0,
            socket_address,
            cb_user_data->port,
            cb_user_data->backlog,
            network_channel_listener_new_callback_socket_setup_server_cb,
            user_data);

    if (fd == -1) {
        return false;
    }

    uint32_t listener_id = cb_user_data->listeners_count + socket_address_index;

    cb_user_data->listeners[listener_id].fd = fd;
    cb_user_data->listeners[listener_id].address.size = socket_address_size;

    memcpy(
            &cb_user_data->listeners[listener_id].address.socket.base,
            socket_address,
            socket_address_size);

    network_io_common_socket_address_str(
            socket_address,
            cb_user_data->listeners[listener_id].address.str,
            sizeof(cb_user_data->listeners[listener_id].address.str));

    LOG_V(TAG, "Created listener for <%s>", cb_user_data->listeners[listener_id].address.str);

    return true;
}

bool network_channel_listener_new(
        char* address,
        uint16_t port,
        network_channel_listener_new_callback_user_data_t *user_data) {
    int res;
    LOG_V(TAG, "Creating listener for <%s:%d>", address, port);

    user_data->port = port;

    res = network_io_common_parse_addresses_foreach(
            address,
            network_channel_listener_new_callback,
            user_data);

    if (res == -1) {
        return false;
    }

    user_data->listeners_count += res;

    return true;
}

network_channel_t* network_channel_new() {
    return (network_channel_t*)xalloc_alloc_zero(sizeof(network_channel_t));
}

void network_channel_free(network_channel_t* network_channel) {
    xalloc_free(network_channel);
}
