#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <arpa/inet.h>

#include "log.h"

uint16_t network_tests_support_search_free_port_ipv4(
        uint16_t start_port) {
    uint16_t port;
    int val = 1;
    struct sockaddr_in address = {0};

    address.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    assert(start_port < UINT16_MAX - 1000);

    bool found = false;
    for(port = start_port; port < start_port + 1000 && !found; port++) {
        address.sin_port = htons(port);

        int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
        if (bind(fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            LOG_DI("Unable to bind ipv4 socket because: %s!", strerror(errno));
        } else {
            found = true;
        }
        close(fd);
    }

    assert(found);

    return port;
}

uint16_t network_tests_support_search_free_port_ipv6(
        uint16_t start_port) {
    uint16_t port;
    int val = 1;
    struct sockaddr_in6 address = {0};

    address.sin6_family = AF_INET6;
    inet_pton(AF_INET6, "::1", &address.sin6_addr);

    assert(start_port < UINT16_MAX - 1000);

    bool found = false;
    for(port = start_port; port < start_port + 1000 && !found; port++) {
        address.sin6_port = htons(port);

        int fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
        setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val));
        if (bind(fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            LOG_DI("Unable to bind ipv6 socket because: %s!", strerror(errno));
        } else {
            found = true;
        }
        close(fd);
    }

    assert(found);

    return port;
}