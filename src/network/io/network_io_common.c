#include <stdint.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include "misc.h"
#include "log.h"

#include "network_io_common.h"

LOG_PRODUCER_CREATE_LOCAL_DEFAULT("network_io_common", network_io_common)

bool network_io_common_socket_set_option(
        int fd,
        int level,
        int option,
        void* value,
        socklen_t value_size) {
    if (setsockopt(fd, level, option, value, value_size) < 0) {
        LOG_E(LOG_PRODUCER_DEFAULT, "Unable to set an option on the socket with fd <%d>", fd);
        LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);

        return false;
    }

    return true;
}

bool network_io_common_socket_bind(
        int fd,
        struct sockaddr *address,
        socklen_t address_size) {
    if (bind(fd, address, address_size) < 0) {
        LOG_E(LOG_PRODUCER_DEFAULT, "Error binding the socket");
        LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);

        return false;
    }

    return true;
}

bool network_io_common_socket_listen(
        int fd,
        uint16_t backlog) {
    if (listen(fd, backlog) < 0) {
        LOG_E(LOG_PRODUCER_DEFAULT, "Error listening on the socket with a backlog of <%d>", backlog);
        LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);
        return false;
    }

    return true;
}

bool network_io_common_socket_setup_server(
        int fd,
        struct sockaddr *address,
        socklen_t address_size,
        uint16_t backlog) {
    int val = 1;

    if (!network_io_common_socket_set_option(
            fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &val,
            sizeof(val))) {
        return false;
    }

    if (address->sa_family == AF_INET6) {
        if (!network_io_common_socket_set_option(
                fd,
                IPPROTO_IPV6,
                IPV6_V6ONLY,
                &val,
                sizeof(val))) {
            return false;
        }
    }

    if (!network_io_common_socket_bind(
            fd,
            (struct sockaddr *)address,
            address_size)) {
        return false;
    }

    if (!network_io_common_socket_listen(
            fd,
            backlog)) {
        return false;
    }

    return true;
}

int network_io_common_socket_tcp4_new() {
    int fd;

    if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        LOG_E(LOG_PRODUCER_DEFAULT, "Unable to create a new IPv4 TCP/IP socket");
        LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);
    }

    return fd;
}

int network_io_common_socket_tcp4_new_server(
        struct sockaddr_in *address,
        uint16_t backlog) {
    int fd;

    fd = network_io_common_socket_tcp4_new();

    if (!network_io_common_socket_setup_server(
            fd,
            (struct sockaddr*)address,
            sizeof(struct sockaddr_in),
            backlog)) {
        return -1;
    }

    return fd;
}

int network_io_common_socket_tcp6_new() {
    int fd;

    if ((fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        LOG_E(LOG_PRODUCER_DEFAULT, "Unable to create a new IPv6 TCP/IP socket");
        LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);
    }

    return fd;
}

int network_io_common_socket_tcp6_new_server(
        struct sockaddr_in6 *address,
        uint16_t backlog) {
    int fd;
    int val = 1;

    fd = network_io_common_socket_tcp6_new();

    if (!network_io_common_socket_setup_server(
            fd,
            (struct sockaddr*)address,
            sizeof(struct sockaddr_in6),
            backlog)) {
        return -1;
    }

    return fd;
}

int network_io_common_socket_new_server(
        int family,
        struct sockaddr *socket_address,
        uint16_t port,
        uint16_t backlog) {
    int fd;

    if (family == AF_INET) {
        struct sockaddr_in* socket_address_ipv4 = (struct sockaddr_in*)socket_address;
        socket_address_ipv4->sin_port = htons(port);
        fd = network_io_common_socket_tcp4_new_server(
                socket_address_ipv4,
                backlog);
    } else if (family == AF_INET6) {
        struct sockaddr_in6* socket_address_ipv6 = (struct sockaddr_in6*)socket_address;
        socket_address_ipv6->sin6_port = htons(port);
        fd = network_io_common_socket_tcp6_new_server(
                socket_address_ipv6,
                backlog);
    } else {
        LOG_E(LOG_PRODUCER_DEFAULT, "Unknown socket family <%d>", family);
        fd = -1;
    }

    return fd;
}

bool network_io_common_parse_addresses_foreach(
        char *address,
        network_io_common_parse_addresses_foreach_callback_t callback,
        void* user_data) {
    struct addrinfo *result, *rp;
    struct addrinfo hints = {0};
    int res;

    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    res = getaddrinfo(address, NULL, &hints, &result);
    if (res != 0) {
        LOG_E(
                LOG_PRODUCER_DEFAULT,
                "Unable to resolve the address <%s> because <%s>",
                address,
                gai_strerror(res));
        return false;
    }

    uint16_t socket_address_index = 0;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if(rp->ai_family != AF_INET && rp->ai_family != AF_INET6) {
            continue;
        }
        callback(
                rp->ai_family,
                rp->ai_addr,
                rp->ai_addrlen,
                socket_address_index++,
                user_data);
    }

    freeaddrinfo(result);
}