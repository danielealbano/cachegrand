#include <catch2/catch.hpp>

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>

#include "../network_tests_support.h"

#include "config.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

TEST_CASE("network/channel/network_channel.c", "[network][network_channel][network_channel]") {
    struct in_addr loopback_ipv4 = { 0 };
    char* loopback_ipv4_str = "127.0.0.1";
    char* any_ipv4_str = "0.0.0.0";
    inet_pton(AF_INET, "127.0.0.1", &loopback_ipv4);

    struct in_addr loopback_ipv6 = { 0 };
    char* loopback_ipv6_str = "::1";
    char* any_ipv6_str = "::";
    inet_pton(AF_INET6, "::1", &loopback_ipv6);

    SECTION("network_channel_client_setup") {
        SECTION("ipv4 socket") {
            int fd = network_io_common_socket_tcp4_new(0);

            REQUIRE(network_channel_client_setup(fd, 0));

            REQUIRE(network_io_common_socket_close(fd, true));
        }

        SECTION("ipv6 socket") {
            int fd = network_io_common_socket_tcp6_new(0);

            REQUIRE(network_channel_client_setup(fd, 0));

            REQUIRE(network_io_common_socket_close(fd, true));
        }

        SECTION("invalid fd") {
            REQUIRE(!network_channel_client_setup(-1, 0));
        }
    }

    SECTION("network_channel_server_setup") {
        SECTION("ipv4 socket") {
            int fd = network_io_common_socket_tcp4_new(0);

            REQUIRE(network_channel_server_setup(fd, 0));

            REQUIRE(network_io_common_socket_close(fd, true));
        }

        SECTION("ipv6 socket") {
            int fd = network_io_common_socket_tcp6_new(0);

            REQUIRE(network_channel_server_setup(fd, 0));

            REQUIRE(network_io_common_socket_close(fd, true));
        }

        SECTION("invalid fd") {
            REQUIRE(!network_channel_server_setup(-1, 0));
        }
    }

    SECTION("network_channel_listener_new_callback") {
        uint16_t socket_port_free_ipv4 =
                network_tests_support_search_free_port_ipv4(9999);
        uint16_t socket_port_free_ipv6 =
                network_tests_support_search_free_port_ipv6(9999);

        network_channel_t test_listeners[10] = { 0 };
        network_channel_listener_new_callback_user_data_t listener_new_cb_user_data = { 0 };
        listener_new_cb_user_data.network_channel_size = sizeof(network_channel_t);

        SECTION("count ipv4 address") {
            listener_new_cb_user_data.listeners = NULL;

            struct sockaddr_in address = {0};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = loopback_ipv4.s_addr;

            REQUIRE(network_channel_listener_new_callback(
                    AF_INET,
                    (struct sockaddr*)&address,
                    sizeof(address),
                    socket_port_free_ipv4,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));
            REQUIRE(listener_new_cb_user_data.listeners_count == 1);
        }

        SECTION("count ipv6 address") {
            listener_new_cb_user_data.listeners = NULL;

            struct sockaddr_in6 address = {
                    AF_INET6,
                    0,
                    0,
                    IN6ADDR_LOOPBACK_INIT,
                    0
            };

            REQUIRE(network_channel_listener_new_callback(
                    AF_INET6,
                    (struct sockaddr*)&address,
                    sizeof(address),
                    socket_port_free_ipv4,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));
            REQUIRE(listener_new_cb_user_data.listeners_count == 1);
        }

        SECTION("ipv4 address") {
            listener_new_cb_user_data.listeners = test_listeners;

            struct sockaddr_in address = {0};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = loopback_ipv4.s_addr;

            REQUIRE(network_channel_listener_new_callback(
                    AF_INET,
                    (struct sockaddr*)&address,
                    sizeof(address),
                    socket_port_free_ipv4,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));

            REQUIRE(listener_new_cb_user_data.listeners[0].fd > 0);
            REQUIRE(listener_new_cb_user_data.listeners[0].address.size == sizeof(struct sockaddr_in));
            REQUIRE(listener_new_cb_user_data.listeners[0].address.socket.ipv4.sin_port == htons(socket_port_free_ipv4));
            REQUIRE(listener_new_cb_user_data.listeners[0].address.socket.ipv4.sin_addr.s_addr == inet_addr(loopback_ipv4_str));

            REQUIRE(network_io_common_socket_close(listener_new_cb_user_data.listeners[0].fd, false));
        }

        SECTION("ipv4 address - non zero listeners_count") {
            listener_new_cb_user_data.listeners_count = 2;
            listener_new_cb_user_data.listeners = test_listeners;

            struct sockaddr_in address = {0};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = loopback_ipv4.s_addr;

            REQUIRE(network_channel_listener_new_callback(
                    AF_INET,
                    (struct sockaddr*)&address,
                    sizeof(address),
                    socket_port_free_ipv4,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));

            REQUIRE(listener_new_cb_user_data.listeners_count == 3);
            REQUIRE(listener_new_cb_user_data.listeners[2].fd > 0);
            REQUIRE(listener_new_cb_user_data.listeners[2].address.size == sizeof(struct sockaddr_in));
            REQUIRE(listener_new_cb_user_data.listeners[2].address.socket.ipv4.sin_port == htons(socket_port_free_ipv4));
            REQUIRE(listener_new_cb_user_data.listeners[2].address.socket.ipv4.sin_addr.s_addr == inet_addr(loopback_ipv4_str));

            REQUIRE(network_io_common_socket_close(listener_new_cb_user_data.listeners[2].fd, false));
        }

        SECTION("ipv6 address") {
            listener_new_cb_user_data.listeners = test_listeners;
            struct in6_addr cmp_addr_temp = IN6ADDR_LOOPBACK_INIT;

            struct sockaddr_in6 address = {
                    AF_INET6,
                    0,
                    0,
                    IN6ADDR_LOOPBACK_INIT,
                    0
            };

            REQUIRE(network_channel_listener_new_callback(
                    AF_INET6,
                    (struct sockaddr*)&address,
                    sizeof(address),
                    socket_port_free_ipv6,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));

            REQUIRE(listener_new_cb_user_data.listeners[0].fd > 0);
            REQUIRE(listener_new_cb_user_data.listeners[0].address.size == sizeof(struct sockaddr_in6));
            REQUIRE(listener_new_cb_user_data.listeners[0].address.socket.ipv6.sin6_port == htons(socket_port_free_ipv6));
            REQUIRE(memcmp(
                    &listener_new_cb_user_data.listeners[0].address.socket.ipv6.sin6_addr,
                    &cmp_addr_temp,
                    sizeof(struct in6_addr)) == 0);

            REQUIRE(network_io_common_socket_close(listener_new_cb_user_data.listeners[0].fd, false));
        }

        SECTION("multiple callback calls") {
            listener_new_cb_user_data.listeners = test_listeners;
            struct in6_addr cmp_addr_temp = IN6ADDR_LOOPBACK_INIT;

            struct sockaddr_in address4 = {0};
            address4.sin_family = AF_INET;
            address4.sin_addr.s_addr = loopback_ipv4.s_addr;

            struct sockaddr_in6 address6 = {
                    AF_INET6,
                    0,
                    0,
                    IN6ADDR_LOOPBACK_INIT,
                    0
            };

            REQUIRE(network_channel_listener_new_callback(
                    AF_INET,
                    (struct sockaddr*)&address4,
                    sizeof(address4),
                    socket_port_free_ipv4,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));

            REQUIRE(network_channel_listener_new_callback(
                    AF_INET6,
                    (struct sockaddr*)&address6,
                    sizeof(address6),
                    socket_port_free_ipv6,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));

            REQUIRE(listener_new_cb_user_data.listeners[0].fd > 0);
            REQUIRE(listener_new_cb_user_data.listeners[0].address.size == sizeof(struct sockaddr_in));
            REQUIRE(listener_new_cb_user_data.listeners[0].address.socket.ipv4.sin_port == htons(socket_port_free_ipv4));
            REQUIRE(listener_new_cb_user_data.listeners[0].address.socket.ipv4.sin_addr.s_addr == inet_addr(loopback_ipv4_str));
            REQUIRE(listener_new_cb_user_data.listeners[1].fd > 0);
            REQUIRE(listener_new_cb_user_data.listeners[1].address.size == sizeof(struct sockaddr_in6));
            REQUIRE(listener_new_cb_user_data.listeners[1].address.socket.ipv6.sin6_port == htons(socket_port_free_ipv6));
            REQUIRE(memcmp(
                    &listener_new_cb_user_data.listeners[1].address.socket.ipv6.sin6_addr,
                    &cmp_addr_temp,
                    sizeof(struct in6_addr)) == 0);

            REQUIRE(network_io_common_socket_close(listener_new_cb_user_data.listeners[0].fd, false));
            REQUIRE(network_io_common_socket_close(listener_new_cb_user_data.listeners[1].fd, false));
        }

        SECTION("unsupported family") {
            listener_new_cb_user_data.listeners = test_listeners;

            struct sockaddr_in address = {0};

            REQUIRE(!network_channel_listener_new_callback(
                    1234,
                    (struct sockaddr*)&address,
                    sizeof(address),
                    socket_port_free_ipv4,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));
        }
    }

    SECTION("network_channel_listener_new") {
        uint16_t socket_port_free_ipv4 =
                network_tests_support_search_free_port_ipv4(9999);
        uint16_t socket_port_free_ipv6 =
                network_tests_support_search_free_port_ipv6(9999);
        network_channel_t test_listeners[10] = { 0 };

        network_channel_listener_new_callback_user_data_t listener_new_cb_user_data = { 0 };
        listener_new_cb_user_data.network_channel_size = sizeof(network_channel_t);

        SECTION("ipv4 loopback") {
            listener_new_cb_user_data.listeners = test_listeners;
            REQUIRE(network_channel_listener_new(
                    loopback_ipv4_str,
                    socket_port_free_ipv4,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));

            REQUIRE(listener_new_cb_user_data.listeners_count == 1);
            REQUIRE(listener_new_cb_user_data.listeners[0].fd > 0);
            REQUIRE(listener_new_cb_user_data.listeners[0].address.size == sizeof(struct sockaddr_in));
            REQUIRE(listener_new_cb_user_data.listeners[0].address.socket.ipv4.sin_port == htons(socket_port_free_ipv4));
            REQUIRE(listener_new_cb_user_data.listeners[0].address.socket.ipv4.sin_addr.s_addr == inet_addr(loopback_ipv4_str));

            REQUIRE(network_io_common_socket_close(listener_new_cb_user_data.listeners[0].fd, false));
        }

        SECTION("ipv4 any") {
            listener_new_cb_user_data.listeners = test_listeners;
            REQUIRE(network_channel_listener_new(
                    any_ipv4_str,
                    socket_port_free_ipv4,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));

            REQUIRE(listener_new_cb_user_data.listeners_count == 1);
            REQUIRE(listener_new_cb_user_data.listeners[0].fd > 0);
            REQUIRE(listener_new_cb_user_data.listeners[0].address.size == sizeof(struct sockaddr_in));
            REQUIRE(listener_new_cb_user_data.listeners[0].address.socket.ipv4.sin_port == htons(socket_port_free_ipv4));
            REQUIRE(listener_new_cb_user_data.listeners[0].address.socket.ipv4.sin_addr.s_addr == INADDR_ANY);

            REQUIRE(network_io_common_socket_close(listener_new_cb_user_data.listeners[0].fd, false));
        }

        SECTION("ipv6 loopback") {
            listener_new_cb_user_data.listeners = test_listeners;
            struct in6_addr cmp_addr_temp = IN6ADDR_LOOPBACK_INIT;

            REQUIRE(network_channel_listener_new(
                    loopback_ipv6_str,
                    socket_port_free_ipv6,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));

            REQUIRE(listener_new_cb_user_data.listeners_count == 1);
            REQUIRE(listener_new_cb_user_data.listeners[0].fd > 0);
            REQUIRE(listener_new_cb_user_data.listeners[0].address.size == sizeof(struct sockaddr_in6));
            REQUIRE(listener_new_cb_user_data.listeners[0].address.socket.ipv6.sin6_port == htons(socket_port_free_ipv6));
            REQUIRE(memcmp(
                    &listener_new_cb_user_data.listeners[0].address.socket.ipv6.sin6_addr,
                    &cmp_addr_temp,
                    sizeof(struct in6_addr)) == 0);

            REQUIRE(network_io_common_socket_close(listener_new_cb_user_data.listeners[0].fd, false));
        }

        SECTION("ipv6 any") {
            listener_new_cb_user_data.listeners = test_listeners;
            struct in6_addr cmp_addr_temp = IN6ADDR_ANY_INIT;

            REQUIRE(network_channel_listener_new(
                    any_ipv6_str,
                    socket_port_free_ipv6,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));

            REQUIRE(listener_new_cb_user_data.listeners_count == 1);
            REQUIRE(listener_new_cb_user_data.listeners[0].fd > 0);
            REQUIRE(listener_new_cb_user_data.listeners[0].address.size == sizeof(struct sockaddr_in6));
            REQUIRE(listener_new_cb_user_data.listeners[0].address.socket.ipv6.sin6_port == htons(socket_port_free_ipv6));
            REQUIRE(memcmp(
                    &listener_new_cb_user_data.listeners[0].address.socket.ipv6.sin6_addr,
                    &cmp_addr_temp,
                    sizeof(struct in6_addr)) == 0);

            REQUIRE(network_io_common_socket_close(listener_new_cb_user_data.listeners[0].fd, false));
        }

        SECTION("multiple listeners") {
            listener_new_cb_user_data.listeners = test_listeners;
            struct in6_addr cmp_addr_temp = IN6ADDR_LOOPBACK_INIT;

            REQUIRE(network_channel_listener_new(
                    "127.0.0.1",
                    socket_port_free_ipv4,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));
            REQUIRE(network_channel_listener_new(
                    "::1",
                    socket_port_free_ipv6,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));

            REQUIRE(listener_new_cb_user_data.listeners_count == 2);

            REQUIRE(listener_new_cb_user_data.listeners[0].fd > 0);
            REQUIRE(listener_new_cb_user_data.listeners[0].address.size == sizeof(struct sockaddr_in));
            REQUIRE(listener_new_cb_user_data.listeners[0].address.socket.ipv4.sin_port == htons(socket_port_free_ipv4));
            REQUIRE(listener_new_cb_user_data.listeners[0].address.socket.ipv4.sin_addr.s_addr == inet_addr(loopback_ipv4_str));
            REQUIRE(listener_new_cb_user_data.listeners[1].fd > 0);
            REQUIRE(listener_new_cb_user_data.listeners[1].address.size == sizeof(struct sockaddr_in6));
            REQUIRE(listener_new_cb_user_data.listeners[1].address.socket.ipv6.sin6_port == htons(socket_port_free_ipv6));
            REQUIRE(memcmp(
                    &listener_new_cb_user_data.listeners[1].address.socket.ipv6.sin6_addr,
                    &cmp_addr_temp,
                    sizeof(struct in6_addr)) == 0);

            REQUIRE(network_io_common_socket_close(listener_new_cb_user_data.listeners[0].fd, false));
            REQUIRE(network_io_common_socket_close(listener_new_cb_user_data.listeners[1].fd, false));
        }

        SECTION("invalid ipv4 address") {
            listener_new_cb_user_data.listeners = test_listeners;
            REQUIRE(network_channel_listener_new(
                    "1.2.3.4",
                    socket_port_free_ipv4,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));

            REQUIRE(listener_new_cb_user_data.listeners_count == 0);
        }

        SECTION("invalid ipv4 address") {
            listener_new_cb_user_data.listeners = test_listeners;
            REQUIRE(network_channel_listener_new(
                    "1.2.3.4",
                    socket_port_free_ipv4,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));

            REQUIRE(listener_new_cb_user_data.listeners_count == 0);
        }

        SECTION("invalid ipv6 address") {
            listener_new_cb_user_data.listeners = test_listeners;
            REQUIRE(network_channel_listener_new(
                    "2001:db8:3333:4444:5555:6666:7777:8888",
                    socket_port_free_ipv6,
                    10,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data));

            REQUIRE(listener_new_cb_user_data.listeners_count == 0);
        }
    }
}
