#include "../../catch.hpp"

#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <arpa/inet.h>

#include "log.h"

#include "network/io/network_io_common.h"

#include "network_io_tests_support.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

void test_network_io_common_parse_addresses_foreach_callback_loopback_ipv4_address(
        int family,
        struct sockaddr *socket_address,
        socklen_t socket_address_size,
        uint16_t socket_address_index,
        void* user_data) {
    REQUIRE(socket_address_index == 0);
    REQUIRE(socket_address->sa_family == AF_INET);
    REQUIRE(((struct sockaddr_in*)socket_address)->sin_addr.s_addr == inet_addr("127.0.0.1"));
}

void test_network_io_common_parse_addresses_foreach_callback_loopback_ipv6_address(
        int family,
        struct sockaddr *socket_address,
        socklen_t socket_address_size,
        uint16_t socket_address_index,
        void* user_data) {
    struct in6_addr addr = {0};
    inet_pton(AF_INET6, "::1", &addr);

    REQUIRE(socket_address_index == 0);
    REQUIRE(socket_address->sa_family == AF_INET6);
    REQUIRE(memcmp(
            (void*)&addr,
            (void*)(&(((struct sockaddr_in6*)socket_address)->sin6_addr)),
            sizeof(addr)) == 0);
}

void test_network_io_common_parse_addresses_foreach_callback_localhost_ipv4_ipv6_addresses(
        int family,
        struct sockaddr *socket_address,
        socklen_t socket_address_size,
        uint16_t socket_address_index,
        void* user_data) {
    if (socket_address->sa_family == AF_INET) {
        ((uint8_t*)user_data)[0] = 1;
    } else if (socket_address->sa_family == AF_INET6) {
        ((uint8_t*)user_data)[1] = 1;
    }
}

TEST_CASE("network/io/network_io_common", "[network][network_io][network_io_common]") {
    struct in_addr loopback_ipv4 = {0};
    struct in_addr loopback_ipv6 = {0};
    uint16_t socket_port_free_ipv4 =
            network_io_tests_support_search_free_port_ipv4(9999);
    uint16_t socket_port_free_ipv6 =
            network_io_tests_support_search_free_port_ipv6(9999);

    inet_pton(AF_INET, "127.0.0.1", &loopback_ipv4);
    inet_pton(AF_INET, "::1", &loopback_ipv6);

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

    SECTION("network_io_common_socket_set_linger") {
        SECTION("valid socket fd") {
            int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            REQUIRE(network_io_common_socket_set_linger(fd, true, 1));

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
            REQUIRE(network_io_common_socket_set_receive_timeout(fd, 2, 10000));
            REQUIRE(getsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &val, &val_size) == 0);
            REQUIRE(val.tv_sec == 2);
            REQUIRE(val.tv_usec == 10000);

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
            REQUIRE(network_io_common_socket_set_send_timeout(fd, 2, 10000));
            REQUIRE(getsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &val, &val_size) == 0);
            REQUIRE(val.tv_sec == 2);
            REQUIRE(val.tv_usec == 10000);

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
    }

    SECTION("network_io_common_socket_setup_server") {
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
                    10));

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
                    10));

            shutdown(fd, SHUT_RDWR);
            close(fd);
        }
    }

    SECTION("network_io_common_socket_tcp4_new") {
        int fd = network_io_common_socket_tcp4_new(0);
        REQUIRE(fd > 0);
        close(fd);
    }

    SECTION("network_io_common_socket_tcp4_new_server") {
        int fd;
        struct sockaddr_in address = {0};
        address.sin_family = AF_INET;
        address.sin_port = htons(socket_port_free_ipv4);
        address.sin_addr.s_addr = loopback_ipv4.s_addr;

        fd = network_io_common_socket_tcp4_new_server(0, &address, 10);

        REQUIRE(fd > 0);

        shutdown(fd, SHUT_RDWR);
        close(fd);
    }

    SECTION("network_io_common_socket_tcp6_new") {
        int fd = network_io_common_socket_tcp6_new(0);
        REQUIRE(fd > 0);
        close(fd);
    }

    SECTION("network_io_common_socket_tcp6_new_server") {
        int fd;
        struct sockaddr_in6 address = {0};
        address.sin6_family = AF_INET6;
        address.sin6_port = htons(socket_port_free_ipv6);
        memcpy(&(address.sin6_addr), (void*)&loopback_ipv6, sizeof(loopback_ipv6));

        fd = network_io_common_socket_tcp6_new_server(0, &address, 10);

        REQUIRE(fd > 0);

        shutdown(fd, SHUT_RDWR);
        close(fd);
    }

    SECTION("network_io_common_socket_new_server") {
        SECTION("valid ipv4 address and port") {
            int fd;
            struct sockaddr_in address = {0};

            address.sin_family = AF_INET;
            address.sin_addr.s_addr = loopback_ipv4.s_addr;

            fd = network_io_common_socket_new_server(
                    AF_INET,
                    (struct sockaddr*)&address,
                    socket_port_free_ipv4,
                    10);

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
                    (struct sockaddr*)&address,
                    socket_port_free_ipv6,
                    10);

            REQUIRE(fd > 0);

            shutdown(fd, SHUT_RDWR);
            close(fd);
        }
    }

    SECTION("network_io_common_socket_close") {
        SECTION("valid socket") {
            int fd;
            struct sockaddr_in address = {0};

            address.sin_family = AF_INET;
            address.sin_addr.s_addr = loopback_ipv4.s_addr;

            fd = network_io_common_socket_new_server(
                    AF_INET,
                    (struct sockaddr*)&address,
                    socket_port_free_ipv4,
                    10);

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
                    test_network_io_common_parse_addresses_foreach_callback_loopback_ipv4_address,
                    NULL));
        }

        SECTION("loopback ipv6 address") {
            REQUIRE(network_io_common_parse_addresses_foreach(
                    "::1",
                    test_network_io_common_parse_addresses_foreach_callback_loopback_ipv6_address,
                    NULL));
        }

        SECTION("localhost ipv4 and ipv6 addresses") {
            uint8_t res[8] = {0};
            REQUIRE(network_io_common_parse_addresses_foreach(
                    "www.google.it",
                    test_network_io_common_parse_addresses_foreach_callback_localhost_ipv4_ipv6_addresses,
                    &res));

            REQUIRE(res[0] == 1);
            REQUIRE(res[1] == 1);
        }
    }
}
