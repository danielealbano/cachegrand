#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <assert.h>

#include "misc.h"
#include "log.h"
#include "network/io/network_io_common.h"

#include "network/channel/network_channel.h"

LOG_PRODUCER_CREATE_DEFAULT("network_channel", network_channel)

bool network_channel_listener_new_callback_socket_setup_server_cb(
        int fd,
        void *user_data) {
    bool error = false;
    network_channel_listener_new_callback_user_data_t *cb_user_data = user_data;

    // TODO: Replace with an eBPF reuseport program, example:
    /**
            // A = raw_smp_processor_id()
            code[0] = new sock_filter { code = BPF_LD | BPF_W | BPF_ABS, k = SKF_AD_OFF + SKF_AD_CPU };
            // return A
            code[1] = new sock_filter { code = BPF_RET | BPF_A };
     */
    if (network_io_common_socket_set_incoming_cpu(fd, cb_user_data->core_index) == false) {
        error = true;
    }

    if (!error && network_io_common_socket_set_reuse_port(fd, true) == false) {
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
    cb_user_data->listeners[listener_id].address_size = socket_address_size;

    memcpy(
            &cb_user_data->listeners[listener_id].address.base,
            socket_address,
            socket_address_size);

    char address_str[INET6_ADDRSTRLEN + 1] = {0};
    in_port_t port;
    if (family == AF_INET) {
        struct sockaddr_in *address = (struct sockaddr_in *)socket_address;
        inet_ntop(AF_INET, &address->sin_addr, address_str, INET_ADDRSTRLEN);
        port = address->sin_port;
    } else {
        struct sockaddr_in6 *address = (struct sockaddr_in6 *)socket_address;
        inet_ntop(AF_INET6, &address->sin6_addr, address_str, INET6_ADDRSTRLEN);
        port = address->sin6_port;
    }

    LOG_V(LOG_PRODUCER_DEFAULT, "Created listener for <%s:%d>", address_str, ntohs(port));

    return true;
}

bool network_channel_listener_new(
        char* address,
        uint16_t port,
        network_channel_listener_new_callback_user_data_t *user_data) {
    int res;
    LOG_V(LOG_PRODUCER_DEFAULT, "Creating listener for <%s:%d>", address, port);

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

