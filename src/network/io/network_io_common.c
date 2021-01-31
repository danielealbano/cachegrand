#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "misc.h"
#include "log.h"

#include "network/protocol/network_protocol.h"
#include "network_io_common.h"

#define TAG "network_io_common"

bool network_io_common_socket_set_option(
        network_io_common_fd_t fd,
        int level,
        int option,
        void* value,
        socklen_t value_size) {
    if (setsockopt(fd, level, option, value, value_size) < 0) {
        LOG_E(TAG, "Unable to set an option on the socket with fd <%d>", fd);
        LOG_E_OS_ERROR(TAG);

        return false;
    }

    return true;
}

bool network_io_common_socket_set_reuse_address(
        network_io_common_fd_t fd,
        bool enable) {
    int val = enable ? 1 : 0;
    return network_io_common_socket_set_option(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
}

bool network_io_common_socket_set_reuse_port(
        network_io_common_fd_t fd,
        bool enable) {
    int val = enable ? 1 : 0;
    return network_io_common_socket_set_option(fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
}

bool network_io_common_socket_set_nodelay(
        network_io_common_fd_t fd,
        bool enable) {
    int val = enable ? 1 : 0;
    return network_io_common_socket_set_option(fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));
}

bool network_io_common_socket_set_quickack(
        network_io_common_fd_t fd,
        bool enable) {
    int val = enable ? 1 : 0;
    return network_io_common_socket_set_option(fd, IPPROTO_TCP, TCP_QUICKACK, &val, sizeof(val));
}

bool network_io_common_socket_set_linger(
        network_io_common_fd_t fd,
        bool enable,
        int seconds) {
    struct linger linger = { enable ? 1 : 0, seconds };
    return network_io_common_socket_set_option(fd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
}

bool network_io_common_socket_set_keepalive(
        network_io_common_fd_t fd,
        bool enable) {
    int val = enable ? 1 : 0;
    return network_io_common_socket_set_option(fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val));
}

bool network_io_common_socket_set_incoming_cpu(
        network_io_common_fd_t fd,
        int cpu) {
    return network_io_common_socket_set_option(fd, SOL_SOCKET, SO_INCOMING_CPU, &cpu, sizeof(cpu));
}

bool network_io_common_socket_set_receive_buffer(
        network_io_common_fd_t fd,
        int size) {
    return network_io_common_socket_set_option(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
}

bool network_io_common_socket_set_send_buffer(
        network_io_common_fd_t fd,
        int size) {
    return network_io_common_socket_set_option(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
}

bool network_io_common_socket_set_receive_timeout(
        network_io_common_fd_t fd,
        long seconds,
        long useconds) {
    struct timeval timeout = { seconds, useconds };
    return network_io_common_socket_set_option(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

bool network_io_common_socket_set_send_timeout(
        network_io_common_fd_t fd,
        long seconds,
        long useconds) {
    struct timeval timeout = { seconds, useconds };
    return network_io_common_socket_set_option(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

bool network_io_common_socket_set_ipv6_only(
        network_io_common_fd_t fd,
        bool enable) {
    int val = enable ? 1 : 0;
    return network_io_common_socket_set_option(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val));
}

bool network_io_common_socket_bind(
        network_io_common_fd_t fd,
        struct sockaddr *address,
        socklen_t address_size) {
    if (bind(fd, address, address_size) < 0) {
        LOG_E(TAG, "Error binding the socket");
        LOG_E_OS_ERROR(TAG);

        return false;
    }

    return true;
}

bool network_io_common_socket_listen(
        network_io_common_fd_t fd,
        uint16_t backlog) {
    if (listen(fd, backlog) < 0) {
        LOG_E(TAG, "Error listening on the socket with a backlog of <%d>", backlog);
        LOG_E_OS_ERROR(TAG);
        return false;
    }

    return true;
}

bool network_io_common_socket_close(
        network_io_common_fd_t fd,
        bool shutdown_may_fail) {
    bool ret = true;
    if (shutdown(fd, SHUT_RDWR) < 0 && !shutdown_may_fail) {
        LOG_E(TAG, "Error shutting-down the socket with fd <%d>", fd);
        LOG_E_OS_ERROR(TAG);
        ret = false;
    }

    // Try to close the socket anyway if the shutdown fails
    if (close(fd)) {
        LOG_E(TAG, "Error closing the socket with fd <%d>", fd);
        LOG_E_OS_ERROR(TAG);
        ret = false;
    }

    return ret;
}

bool network_io_common_socket_setup_server(
        network_io_common_fd_t fd,
        struct sockaddr *address,
        socklen_t address_size,
        uint16_t backlog,
        network_io_common_socket_setup_server_cb_t socket_setup_server_cb,
        void *socket_setup_server_cb_user_data) {
    int val = 1;
    bool error = false;

    if (!network_io_common_socket_set_reuse_address(fd, true)) {
        error = true;
    }

    if (address->sa_family == AF_INET6) {
        if (!network_io_common_socket_set_option(
                fd,
                IPPROTO_IPV6,
                IPV6_V6ONLY,
                &val,
                sizeof(val))) {
            error = true;
        }
    }

    if (!error && socket_setup_server_cb != NULL) {
        if (!socket_setup_server_cb(fd, socket_setup_server_cb_user_data)) {
            error = true;
        }
    }

    if (!error && !network_io_common_socket_bind(
            fd,
            (struct sockaddr *)address,
            address_size)) {
        error = true;
    }

    if (!error && !network_io_common_socket_listen(
            fd,
            backlog)) {
        error = true;
    }

    if (error) {
        network_io_common_socket_close(fd, true);
    }

    return !error;
}

int network_io_common_socket_tcp4_new(
        int flags) {
    network_io_common_fd_t fd;

    if ((fd = socket(AF_INET, SOCK_STREAM | flags, IPPROTO_TCP)) < 0) {
        LOG_E(TAG, "Unable to create a new IPv4 TCP/IP socket");
        LOG_E_OS_ERROR(TAG);
    }

    return fd;
}

int network_io_common_socket_tcp4_new_server(
        int flags,
        struct sockaddr_in *address,
        uint16_t backlog,
        network_io_common_socket_setup_server_cb_t socket_setup_server_cb,
        void *socket_setup_server_cb_user_data) {
    network_io_common_fd_t fd;

    if ((fd = network_io_common_socket_tcp4_new(flags)) < 0) {
        return -1;
    }

    if (!network_io_common_socket_setup_server(
            fd,
            (struct sockaddr*)address,
            sizeof(struct sockaddr_in),
            backlog,
            socket_setup_server_cb,
            socket_setup_server_cb_user_data)) {
        return -1;
    }

    return fd;
}

int network_io_common_socket_tcp6_new(
        int flags) {
    network_io_common_fd_t fd;

    if ((fd = socket(AF_INET6, SOCK_STREAM | flags, IPPROTO_TCP)) < 0) {
        LOG_E(TAG, "Unable to create a new IPv6 TCP/IP socket");
        LOG_E_OS_ERROR(TAG);
    }

    return fd;
}

int network_io_common_socket_tcp6_new_server(
        int flags,
        struct sockaddr_in6 *address,
        uint16_t backlog,
        network_io_common_socket_setup_server_cb_t socket_setup_server_cb,
        void *socket_setup_server_cb_user_data) {
    network_io_common_fd_t fd;

    if ((fd = network_io_common_socket_tcp6_new(flags)) < 0) {
        return -1;
    }

    if (!network_io_common_socket_setup_server(
            fd,
            (struct sockaddr*)address,
            sizeof(struct sockaddr_in6),
            backlog,
            socket_setup_server_cb,
            socket_setup_server_cb_user_data)) {
        return -1;
    }

    return fd;
}

int network_io_common_socket_new_server(
        int family,
        int flags,
        struct sockaddr *socket_address,
        uint16_t port,
        uint16_t backlog,
        network_io_common_socket_setup_server_cb_t socket_setup_server_cb,
        void *socket_setup_server_cb_user_data) {
    network_io_common_fd_t fd;

    if (family == AF_INET) {
        struct sockaddr_in* socket_address_ipv4 = (struct sockaddr_in*)socket_address;
        socket_address_ipv4->sin_port = htons(port);
        fd = network_io_common_socket_tcp4_new_server(
                flags,
                socket_address_ipv4,
                backlog,
                socket_setup_server_cb,
                socket_setup_server_cb_user_data);
    } else if (family == AF_INET6) {
        struct sockaddr_in6* socket_address_ipv6 = (struct sockaddr_in6*)socket_address;
        socket_address_ipv6->sin6_port = htons(port);
        fd = network_io_common_socket_tcp6_new_server(
                flags,
                socket_address_ipv6,
                backlog,
                socket_setup_server_cb,
                socket_setup_server_cb_user_data);
    } else {
        LOG_E(TAG, "Unknown socket family <%d>", family);
        fd = -1;
    }

    return fd;
}

uint32_t network_io_common_parse_addresses_foreach(
        char *address,
        network_io_common_parse_addresses_foreach_callback_t callback,
        network_protocols_t protocol,
        void* user_data) {
    struct addrinfo *result, *rp;
    struct addrinfo hints = {0};
    int res;

    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    res = getaddrinfo(address, "0", &hints, &result);
    if (res != 0) {
        LOG_E(
                TAG,
                "Unable to resolve the address <%s> because <%s>",
                address,
                gai_strerror(res));
        return -1;
    }

    uint16_t socket_address_index = 0;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if(rp->ai_family != AF_INET && rp->ai_family != AF_INET6) {
            continue;
        }

        if (callback(
                rp->ai_family,
                rp->ai_addr,
                rp->ai_addrlen,
                socket_address_index,
                protocol,
                user_data)) {
            socket_address_index++;
        }
    }

    freeaddrinfo(result);

    return socket_address_index;
}

char* network_io_common_socket_address_str(
        struct sockaddr* address,
        char* buffer,
        size_t buffer_len) {
    in_port_t port;

    if (address->sa_family == AF_INET) {
        if (buffer_len < INET_ADDRSTRLEN + 1) {
            return NULL;
        }

        struct sockaddr_in *address_ipv4 = (struct sockaddr_in *)address;
        inet_ntop(AF_INET, &address_ipv4->sin_addr, buffer, INET_ADDRSTRLEN);
        port = address_ipv4->sin_port;
    } else if (address->sa_family == AF_INET6) {
        if (buffer_len < INET6_ADDRSTRLEN + 1) {
            return NULL;
        }

        struct sockaddr_in6 *address_ipv6 = (struct sockaddr_in6 *)address;
        inet_ntop(AF_INET6, &address_ipv6->sin6_addr, buffer, INET6_ADDRSTRLEN);
        port = address_ipv6->sin6_port;
    } else {
        return NULL;
    }

    snprintf(
            buffer + strlen(buffer),
            buffer_len - strlen(buffer) - 1,
            ":%u",
            ntohs(port));

    return buffer;
}