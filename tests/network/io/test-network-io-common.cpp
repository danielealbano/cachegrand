/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "protocol/redis/protocol_redis.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"

#include "../network_tests_support.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

int test_port = 123;
int test_backlog = 234;
int test_user_data = 345;

bool test_network_io_common_parse_addresses_foreach_callback_loopback_ipv4_address(
        int family,
        struct sockaddr *socket_address,
        socklen_t socket_address_size,
        uint16_t port,
        uint16_t backlog,
        network_protocols_t protocol,
        void* user_data) {
    REQUIRE(port == test_port);
    REQUIRE(backlog == test_backlog);
    REQUIRE(user_data == (void*)&test_user_data);
    REQUIRE(socket_address->sa_family == AF_INET);
    REQUIRE(((struct sockaddr_in*)socket_address)->sin_addr.s_addr == inet_addr("127.0.0.1"));

    return true;
}

bool test_network_io_common_parse_addresses_foreach_callback_loopback_ipv6_address(
        int family,
        struct sockaddr *socket_address,
        socklen_t socket_address_size,
        uint16_t port,
        uint16_t backlog,
        network_protocols_t protocol,
        void* user_data) {
    struct in6_addr addr = {0};
    inet_pton(AF_INET6, "::1", &addr);

    REQUIRE(port == test_port);
    REQUIRE(backlog == test_backlog);
    REQUIRE(user_data == (void*)&test_user_data);
    REQUIRE(socket_address->sa_family == AF_INET6);
    REQUIRE(memcmp(
            (void*)&addr,
            (void*)(&(((struct sockaddr_in6*)socket_address)->sin6_addr)),
            sizeof(addr)) == 0);

    return true;
}

bool test_network_io_common_parse_addresses_foreach_callback_localhost_ipv4_ipv6_addresses(
        int family,
        struct sockaddr *socket_address,
        socklen_t socket_address_size,
        uint16_t port,
        uint16_t backlog,
        network_protocols_t protocol,
        void* user_data) {
    if (socket_address->sa_family == AF_INET) {
        ((uint8_t*)user_data)[0] = 1;
    } else if (socket_address->sa_family == AF_INET6) {
        ((uint8_t*)user_data)[1] = 1;
    }

    return true;
}

TEST_CASE("network/io/network_io_common.c", "[network][network_io][network_io_common]") {
    struct in_addr loopback_ipv4 = { 0 };
    struct in_addr loopback_ipv6 = { 0 };

    inet_pton(AF_INET, "127.0.0.1", &loopback_ipv4);
    inet_pton(AF_INET6, "::1", &loopback_ipv6);

    SECTION("network_io_common_socket_set_option") {
        SECTION("valid option") {
            int val = 1;
            socklen_t val_size = sizeof(val);
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_option(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)));
            REQUIRE(getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, &val_size) == 0);

            close(fd);
        }

        SECTION("invalid option") {
            int val = 1;
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(!network_io_common_socket_set_option(fd, SOL_SOCKET, -1, &val, sizeof(val)));
            REQUIRE(errno == ENOPROTOOPT);

            close(fd);
        }

        SECTION("invalid value") {
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(!network_io_common_socket_set_option(fd, SOL_SOCKET, SO_REUSEADDR, NULL, 0));

            close(fd);
        }

        SECTION("invalid fd") {
            int val = 1;

            REQUIRE(!network_io_common_socket_set_option(-1, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)));
        }

        SECTION("not an fd socket") {
            int val = 1;

            REQUIRE(!network_io_common_socket_set_option(1, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)));
        }
    }

    SECTION("network_io_common_socket_set_reuse_address") {
        SECTION("valid socket fd") {
            int val;
            socklen_t val_size = sizeof(val);
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_reuse_address(fd, true));
            REQUIRE(getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, &val_size) == 0);
            REQUIRE(val == 1);

            close(fd);
        }

        SECTION("invalid socket fd") {
            REQUIRE(!network_io_common_socket_set_reuse_address(-1, true));
        }
    }

    SECTION("network_io_common_socket_set_reuse_port") {
        SECTION("valid socket fd") {
            int val;
            socklen_t val_size = sizeof(val);
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_reuse_port(fd, true));
            REQUIRE(getsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &val, &val_size) == 0);
            REQUIRE(val == 1);

            close(fd);
        }

        SECTION("invalid socket fd") {
            REQUIRE(!network_io_common_socket_set_reuse_port(-1, true));
        }
    }

    SECTION("network_io_common_socket_set_nodelay") {
        SECTION("valid socket fd") {
            int val;
            socklen_t val_size = sizeof(val);
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_nodelay(fd, true));
            REQUIRE(getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, &val_size) == 0);
            REQUIRE(val == 1);

            close(fd);
        }

        SECTION("invalid socket fd") {
            REQUIRE(!network_io_common_socket_set_nodelay(-1, true));
        }
    }

    SECTION("network_io_common_socket_set_quickack") {
        SECTION("valid socket fd") {
            int val;
            socklen_t val_size = sizeof(val);
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_quickack(fd, true));
            REQUIRE(getsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &val, &val_size) == 0);
            REQUIRE(val == 1);

            close(fd);
        }

        SECTION("invalid socket fd") {
            REQUIRE(!network_io_common_socket_set_quickack(-1, true));
        }
    }

    SECTION("network_io_common_socket_set_linger") {
        SECTION("valid socket fd") {
            struct linger val = { 0 };
            socklen_t val_size = sizeof(val);
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_linger(fd, true, 2));
            REQUIRE(getsockopt(fd, SOL_SOCKET, SO_LINGER, &val, &val_size) == 0);
            REQUIRE(val.l_onoff == 1);
            REQUIRE(val.l_linger == 2);

            close(fd);
        }

        SECTION("invalid socket fd") {
            REQUIRE(!network_io_common_socket_set_linger(-1, true, 1));
        }
    }

    SECTION("network_io_common_socket_set_keepalive") {
        SECTION("valid socket fd") {
            int val;
            socklen_t val_size = sizeof(val);
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_keepalive(fd, true));
            REQUIRE(getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &val, &val_size) == 0);
            REQUIRE(val == 1);

            close(fd);
        }

        SECTION("invalid socket fd") {
            REQUIRE(!network_io_common_socket_set_keepalive(-1, true));
        }
    }

    SECTION("network_io_common_socket_set_incoming_cpu") {
        SECTION("valid socket fd") {
            int val;
            socklen_t val_size = sizeof(val);
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_incoming_cpu(fd, 1));
            REQUIRE(getsockopt(fd, SOL_SOCKET, SO_INCOMING_CPU, &val, &val_size) == 0);
            REQUIRE(val == 1);

            close(fd);
        }

        SECTION("invalid socket fd") {
            REQUIRE(!network_io_common_socket_set_incoming_cpu(-1, 1));
        }
    }

    SECTION("network_io_common_socket_set_receive_buffer") {
        SECTION("valid socket fd") {
            int val;
            socklen_t val_size = sizeof(val);
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_receive_buffer(fd, 8192));
            REQUIRE(getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &val, &val_size) == 0);
            REQUIRE(val == 8192 * 2);

            close(fd);
        }

        SECTION("invalid socket fd") {
            REQUIRE(!network_io_common_socket_set_receive_buffer(-1, 8192));
        }
    }

    SECTION("network_io_common_socket_set_send_buffer") {
        SECTION("valid socket fd") {
            int val;
            socklen_t val_size = sizeof(val);
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_send_buffer(fd, 8192));
            REQUIRE(getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &val, &val_size) == 0);
            REQUIRE(val == 8192 * 2);

            close(fd);
        }

        SECTION("invalid socket fd") {
            REQUIRE(!network_io_common_socket_set_send_buffer(-1, 8192));
        }
    }

    SECTION("network_io_common_socket_set_receive_timeout") {
        SECTION("valid socket fd") {
            struct timeval val = { 0 };
            socklen_t val_size = sizeof(val);
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_receive_timeout(fd, 2, 0));
            REQUIRE(getsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &val, &val_size) == 0);
            REQUIRE(val.tv_sec == 2);

            close(fd);
        }

        SECTION("invalid socket fd") {
            REQUIRE(!network_io_common_socket_set_receive_timeout(-1, 1, 0));
        }
    }

    SECTION("network_io_common_socket_set_send_timeout") {
        SECTION("valid socket fd") {
            struct timeval val = { 0 };
            socklen_t val_size = sizeof(val);
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_send_timeout(fd, 2, 0));
            REQUIRE(getsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &val, &val_size) == 0);
            REQUIRE(val.tv_sec == 2);

            close(fd);
        }

        SECTION("invalid socket fd") {
            REQUIRE(!network_io_common_socket_set_send_timeout(-1, 1, 0));
        }
    }

    SECTION("network_io_common_socket_set_ipv6_only") {
        SECTION("valid socket fd") {
            int val;
            socklen_t val_size = sizeof(val);
            int fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_ipv6_only(fd, true));
            REQUIRE(getsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, &val_size) == 0);
            REQUIRE(val == 1);

            close(fd);
        }

        SECTION("invalid socket fd") {
            REQUIRE(!network_io_common_socket_set_ipv6_only(-1, true));
        }

        SECTION("not ipv6 socket fd") {
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(!network_io_common_socket_set_ipv6_only(fd, true));

            close(fd);
        }
    }

    SECTION("network_io_common_socket_bind") {
        uint16_t socket_port_free_ipv4 =
                network_tests_support_search_free_port_ipv4(9999);
        uint16_t socket_port_free_ipv6 =
                network_tests_support_search_free_port_ipv6(9999);

        SECTION("valid ipv4 address and port") {
            int val = 1;
            struct sockaddr_in address = {0};

            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            address.sin_family = AF_INET;
            address.sin_port = htons(socket_port_free_ipv4);
            address.sin_addr.s_addr = loopback_ipv4.s_addr;

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_option(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)));
            REQUIRE(network_io_common_socket_bind(fd, (struct sockaddr*)&address, sizeof(address)));

            close(fd);
        }

        SECTION("valid ipv6 address and port") {
            int val = 1;
            struct sockaddr_in6 address = {0};

            int fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

            address.sin6_family = AF_INET6;
            address.sin6_port = htons(socket_port_free_ipv6);
            memcpy(&(address.sin6_addr), (void*)&loopback_ipv6, sizeof(loopback_ipv6));

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_option(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)));
            REQUIRE(network_io_common_socket_set_option(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val)));
            REQUIRE(network_io_common_socket_bind(fd, (struct sockaddr*)&address, sizeof(address)));

            close(fd);
        }

        SECTION("invalid ipv4 address") {
            int val = 1;
            struct sockaddr_in address = {0};

            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            address.sin_family = AF_INET;
            address.sin_port = htons(socket_port_free_ipv4);
            address.sin_addr.s_addr = inet_addr("1.1.1.1");

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_option(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)));
            REQUIRE(!network_io_common_socket_bind(fd, (struct sockaddr*)&address, sizeof(address)));

            close(fd);
        }

        SECTION("invalid socket fd") {
            struct sockaddr_in address = {0};

            address.sin_family = AF_INET;
            address.sin_port = htons(socket_port_free_ipv4);
            address.sin_addr.s_addr = loopback_ipv4.s_addr;

            REQUIRE(!network_io_common_socket_bind(-1, (struct sockaddr*)&address, sizeof(address)));
        }

        SECTION("not a socket fd") {
            struct sockaddr_in address = {0};

            address.sin_family = AF_INET;
            address.sin_port = htons(socket_port_free_ipv4);
            address.sin_addr.s_addr = loopback_ipv4.s_addr;

            REQUIRE(!network_io_common_socket_bind(1, (struct sockaddr*)&address, sizeof(address)));
        }
    }

    SECTION("network_io_common_socket_listen") {
        uint16_t socket_port_free_ipv4 =
                network_tests_support_search_free_port_ipv4(9999);
        uint16_t socket_port_free_ipv6 =
                network_tests_support_search_free_port_ipv6(9999);

        SECTION("valid ipv4 address and port") {
            int val = 1;
            socklen_t val_size = sizeof(val);
            struct sockaddr_in address = {0};

            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            address.sin_family = AF_INET;
            address.sin_port = htons(socket_port_free_ipv4);
            address.sin_addr.s_addr = loopback_ipv4.s_addr;

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_option(fd, SOL_SOCKET, SO_REUSEADDR, &val, val_size));
            REQUIRE(network_io_common_socket_bind(fd, (struct sockaddr*)&address, sizeof(address)));
            REQUIRE(network_io_common_socket_listen(fd, 10));
            REQUIRE(getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &val, &val_size) == 0);
            REQUIRE(val == 1);

            shutdown(fd, SHUT_RDWR);
            close(fd);
        }

        SECTION("valid ipv6 address and port") {
            int val = 1;
            socklen_t val_size = sizeof(val);

            struct sockaddr_in6 address = {0};

            int fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

            address.sin6_family = AF_INET6;
            address.sin6_port = htons(socket_port_free_ipv6);
            memcpy(&(address.sin6_addr), (void*)&loopback_ipv6, sizeof(loopback_ipv6));

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_set_option(fd, SOL_SOCKET, SO_REUSEADDR, &val, val_size));
            REQUIRE(network_io_common_socket_set_option(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, val_size));
            REQUIRE(network_io_common_socket_bind(fd, (struct sockaddr*)&address, sizeof(address)));
            REQUIRE(network_io_common_socket_listen(fd, 10));
            REQUIRE(getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &val, &val_size) == 0);
            REQUIRE(val == 1);

            shutdown(fd, SHUT_RDWR);
            close(fd);
        }

        SECTION("listen on used port") {
            int val = 1;
            struct sockaddr_in address = {0};

            int fd1 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            int fd2 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            address.sin_family = AF_INET;
            address.sin_port = htons(socket_port_free_ipv4);
            address.sin_addr.s_addr = loopback_ipv4.s_addr;

            REQUIRE(fd1 > 0);
            REQUIRE(network_io_common_socket_set_option(fd1, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)));
            REQUIRE(network_io_common_socket_bind(fd1, (struct sockaddr*)&address, sizeof(address)));
            REQUIRE(network_io_common_socket_listen(fd1, 10));

            REQUIRE(fd2 > 0);
            REQUIRE(network_io_common_socket_set_option(fd2, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)));
            REQUIRE(!network_io_common_socket_bind(fd2, (struct sockaddr*)&address, sizeof(address)));

            shutdown(fd1, SHUT_RDWR);
            close(fd1);
            close(fd2);
        }

        SECTION("invalid fd") {
            REQUIRE(!network_io_common_socket_listen(-1, 10));
        }

        SECTION("not socket fd") {
            REQUIRE(!network_io_common_socket_listen(1, 10));
        }
    }

    SECTION("network_io_common_socket_setup_server") {
        uint16_t socket_port_free_ipv4 =
                network_tests_support_search_free_port_ipv4(9999);
        uint16_t socket_port_free_ipv6 =
                network_tests_support_search_free_port_ipv6(9999);

        SECTION("valid ipv4 address and port") {
            struct sockaddr_in address = {0};

            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            address.sin_family = AF_INET;
            address.sin_port = htons(socket_port_free_ipv4);
            address.sin_addr.s_addr = loopback_ipv4.s_addr;

            REQUIRE(network_io_common_socket_setup_server(
                    fd,
                    (struct sockaddr*)&address,
                    sizeof(address),
                    10,
                    NULL,
                    NULL));

            shutdown(fd, SHUT_RDWR);
            close(fd);
        }

        SECTION("valid ipv6 address and port") {
            struct sockaddr_in6 address = {0};

            int fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

            address.sin6_family = AF_INET6;
            address.sin6_port = htons(socket_port_free_ipv6);
            memcpy(&(address.sin6_addr), (void*)&loopback_ipv6, sizeof(loopback_ipv6));

            REQUIRE(network_io_common_socket_setup_server(
                    fd,
                    (struct sockaddr*)&address,
                    sizeof(address),
                    10,
                    NULL,
                    NULL));

            shutdown(fd, SHUT_RDWR);
            close(fd);
        }

        SECTION("invalid fd") {
            struct sockaddr_in address = {0};

            address.sin_family = AF_INET;
            address.sin_port = htons(socket_port_free_ipv4);
            address.sin_addr.s_addr = loopback_ipv4.s_addr;

            REQUIRE(!network_io_common_socket_setup_server(
                    -1,
                    (struct sockaddr*)&address,
                    sizeof(address),
                    10,
                    NULL,
                    NULL));
        }
    }

    SECTION("network_io_common_socket_tcp4_new") {
        SECTION("allowed flags") {
            int fd = network_io_common_socket_tcp4_new(0);
            REQUIRE(fd > 0);
            close(fd);
        }

        SECTION("invalid flags") {
            REQUIRE(network_io_common_socket_tcp4_new(-1) < 0);
        }
    }

    SECTION("network_io_common_socket_tcp4_new_server") {
        uint16_t socket_port_free_ipv4 =
                network_tests_support_search_free_port_ipv4(9999);
        uint16_t socket_port_free_ipv6 =
                network_tests_support_search_free_port_ipv6(9999);

        int fd;
        struct sockaddr_in address = {0};
        address.sin_family = AF_INET;
        address.sin_port = htons(socket_port_free_ipv4);
        address.sin_addr.s_addr = loopback_ipv4.s_addr;

        fd = network_io_common_socket_tcp4_new_server(
                0,
                &address,
                10,
                NULL,
                NULL);

        REQUIRE(fd > 0);

        shutdown(fd, SHUT_RDWR);
        close(fd);
    }

    SECTION("network_io_common_socket_tcp6_new") {
        SECTION("allowed flags") {
            int fd = network_io_common_socket_tcp6_new(0);
            REQUIRE(fd > 0);
            close(fd);
        }

        SECTION("invalid flags") {
            REQUIRE(network_io_common_socket_tcp6_new(-1) < 0);
        }
    }

    SECTION("network_io_common_socket_tcp6_new_server") {
        uint16_t socket_port_free_ipv4 =
                network_tests_support_search_free_port_ipv4(9999);
        uint16_t socket_port_free_ipv6 =
                network_tests_support_search_free_port_ipv6(9999);

        int fd;
        struct sockaddr_in6 address = {0};
        address.sin6_family = AF_INET6;
        address.sin6_port = htons(socket_port_free_ipv6);
        memcpy(&(address.sin6_addr), (void*)&loopback_ipv6, sizeof(loopback_ipv6));

        fd = network_io_common_socket_tcp6_new_server(
                0,
                &address,
                10,
                NULL,
                NULL);

        REQUIRE(fd > 0);

        shutdown(fd, SHUT_RDWR);
        close(fd);
    }

    SECTION("network_io_common_socket_new_server") {
        uint16_t socket_port_free_ipv4 =
                network_tests_support_search_free_port_ipv4(9999);
        uint16_t socket_port_free_ipv6 =
                network_tests_support_search_free_port_ipv6(9999);

        SECTION("valid ipv4 address and port") {
            int fd;
            struct sockaddr_in address = {0};

            address.sin_family = AF_INET;
            address.sin_addr.s_addr = loopback_ipv4.s_addr;

            fd = network_io_common_socket_new_server(
                    AF_INET,
                    0,
                    (struct sockaddr*)&address,
                    socket_port_free_ipv4,
                    10,
                    NULL,
                    NULL);

            REQUIRE(fd > 0);

            shutdown(fd, SHUT_RDWR);
            close(fd);
        }

        SECTION("valid ipv6 address and port") {
            int fd;
            struct sockaddr_in6 address = {0};

            address.sin6_family = AF_INET6;
            memcpy(&(address.sin6_addr), (void*)&loopback_ipv6, sizeof(loopback_ipv6));

            fd = network_io_common_socket_new_server(
                    AF_INET6,
                    0,
                    (struct sockaddr*)&address,
                    socket_port_free_ipv6,
                    10,
                    NULL,
                    NULL);

            REQUIRE(fd > 0);

            shutdown(fd, SHUT_RDWR);
            close(fd);
        }

        SECTION("invalid family") {
            REQUIRE(network_io_common_socket_new_server(
                    12345,
                    0,
                    NULL,
                    0,
                    10,
                    NULL,
                    NULL) == -1);
        }
    }

    SECTION("network_io_common_socket_close") {
        uint16_t socket_port_free_ipv4 =
                network_tests_support_search_free_port_ipv4(9999);
        uint16_t socket_port_free_ipv6 =
                network_tests_support_search_free_port_ipv6(9999);

        SECTION("valid socket") {
            int fd;
            struct sockaddr_in address = {0};

            address.sin_family = AF_INET;
            address.sin_addr.s_addr = loopback_ipv4.s_addr;

            fd = network_io_common_socket_new_server(
                    AF_INET,
                    0,
                    (struct sockaddr*)&address,
                    socket_port_free_ipv4,
                    10,
                    NULL,
                    NULL);

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_close(fd, false));

            close(fd);
        }

        SECTION("not connected socket") {
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(fd > 0);
            REQUIRE(network_io_common_socket_close(fd, true));

            close(fd);
        }

        SECTION("invalid socket") {
            REQUIRE(!network_io_common_socket_close(-1, false));
        }
    }

    SECTION("network_io_common_parse_addresses_foreach") {
        SECTION("loopback ipv4 address") {
            REQUIRE(network_io_common_parse_addresses_foreach(
                    "127.0.0.1",
                    test_port,
                    test_backlog,
                    test_network_io_common_parse_addresses_foreach_callback_loopback_ipv4_address,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    (void*)&test_user_data) == 1);
        }

        SECTION("loopback ipv6 address") {
            REQUIRE(network_io_common_parse_addresses_foreach(
                    "::1",
                    test_port,
                    test_backlog,
                    test_network_io_common_parse_addresses_foreach_callback_loopback_ipv6_address,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    (void*)&test_user_data) == 1);
        }

        SECTION("localhost ipv4 and ipv6 addresses") {
            uint8_t res[8] = {0};
            REQUIRE(network_io_common_parse_addresses_foreach(
                    "www.google.it",
                    test_port,
                    test_backlog,
                    test_network_io_common_parse_addresses_foreach_callback_localhost_ipv4_ipv6_addresses,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &res) == 2);

            REQUIRE(res[0] == 1);
            REQUIRE(res[1] == 1);
        }

        SECTION("invalid address") {
            REQUIRE(network_io_common_parse_addresses_foreach(
                    "this is an invalid address! should return -1",
                    test_port,
                    test_backlog,
                    test_network_io_common_parse_addresses_foreach_callback_loopback_ipv6_address,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    (void*)&test_user_data) == -1);
        }
    }
}
