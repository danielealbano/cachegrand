/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdbool.h>
#include <stdint.h>
#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>
#include <arpa/inet.h>

#include "random.h"

#include "network_tests_support.h"

uint16_t network_tests_support_search_free_port_ipv4() {
    uint16_t port;
    int val = 1;
    struct linger linger = { 0, 0 };
    struct sockaddr_in address = {0};

    address.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    bool found = false;
    int port_start = (uint16_t)(random_generate() % (UINT16_MAX - 1024)) + 1024;
    for(int port_index = port_start; port_index < (port_start + UINT16_MAX) && !found; port_index++) {
        port = port_index % UINT16_MAX;
        if (port < 1024) {
            continue;
        }

        address.sin_port = htons(port);

        int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
        setsockopt(fd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));

        if (bind(fd, (struct sockaddr*)&address, sizeof(address)) == 0) {
            found = true;
        }

        close(fd);
    }

    assert(found);

    return port;
}

uint16_t network_tests_support_search_free_port_ipv6() {
    uint16_t port;
    int val = 1;
    struct linger linger = { 0, 0 };
    struct sockaddr_in6 address = {0};

    address.sin6_family = AF_INET6;
    inet_pton(AF_INET6, "::1", &address.sin6_addr);

    bool found = false;
    int port_start = (uint16_t)(random_generate() % (UINT16_MAX - 1024)) + 1024;
    for(int port_index = port_start; port_index < (port_start + UINT16_MAX) && !found; port_index++) {
        port = port_index % UINT16_MAX;
        if (port < 1024) {
            continue;
        }
        address.sin6_port = htons(port);

        int fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
        setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
        setsockopt(fd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
        setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val));

        if (bind(fd, (struct sockaddr*)&address, sizeof(address)) == 0) {
            found = true;
        }

        close(fd);
    }

    assert(found);

    return port;
}