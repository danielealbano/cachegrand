#include <catch2/catch.hpp>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#include <liburing.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "support/io_uring/io_uring_support.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"

#include "../../network/network_tests_support.h"

TEST_CASE("support/io_uring/io_uring_support.c", "[support][io_uring][io_uring_support]") {
    struct in_addr loopback_ipv4 = {0};
    struct in_addr loopback_ipv6 = {0};

    inet_pton(AF_INET, "127.0.0.1", &loopback_ipv4);
    inet_pton(AF_INET, "::1", &loopback_ipv6);

    SECTION("io_uring_support_init") {
        SECTION("null params") {
            io_uring_t *ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            io_uring_support_free(ring);
        }

        SECTION("with params") {
            io_uring_params_t params = {0};
            io_uring_t *ring = io_uring_support_init(10, &params, NULL);

            REQUIRE(ring != NULL);
            REQUIRE(params.features != 0);

            io_uring_support_free(ring);
        }

        SECTION("with params and features") {
            io_uring_params_t params = {0};
            uint32_t features = 0;
            io_uring_t *ring = io_uring_support_init(10, &params, &features);

            REQUIRE(ring != NULL);
            REQUIRE(params.features != 0);
            REQUIRE(params.features == features);

            io_uring_support_free(ring);
        }

        SECTION("without params but with features") {
            uint32_t features = 0;
            io_uring_t *ring = io_uring_support_init(10, NULL, &features);

            REQUIRE(ring != NULL);
            REQUIRE(features != 0);

            io_uring_support_free(ring);
        }

        SECTION("fail because too many entries") {
            io_uring_t *ring = io_uring_support_init(64000, NULL, NULL);

            REQUIRE(ring == NULL);
        }
    }

    SECTION("io_uring_support_probe_opcode") {
        SECTION("valid opcode") {
            REQUIRE(io_uring_support_probe_opcode(IORING_OP_READV));
        }

        SECTION("invalid opcode") {
            REQUIRE(!io_uring_support_probe_opcode(UINT8_MAX));
        }
    }

    SECTION("io_uring_support_get_sqe") {
        SECTION("fetch one") {
            io_uring_t *ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);
            REQUIRE(io_uring_support_get_sqe(ring) != NULL);

            io_uring_support_free(ring);
        }

        SECTION("overflow") {
            io_uring_t *ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            for(uint8_t i = 0; i < 16; i++) {
                REQUIRE(io_uring_support_get_sqe(ring) != NULL);
            }
            REQUIRE(io_uring_support_get_sqe(ring) == NULL);

            io_uring_support_free(ring);
        }
    }

    SECTION("io_uring_support_sqe_submit") {
        io_uring_sqe_t *sqe;
        io_uring_cqe_t *cqe;

        io_uring_t *ring = io_uring_support_init(10, NULL, NULL);

        REQUIRE(ring != NULL);

        sqe = io_uring_support_get_sqe(ring);
        REQUIRE(sqe != NULL);
        io_uring_prep_nop(sqe);
        sqe->user_data = 1234;

        io_uring_support_sqe_submit(ring);

        io_uring_wait_cqe(ring, &cqe);
        REQUIRE(cqe->flags == 0);
        REQUIRE(cqe->res == 0);
        REQUIRE(cqe->user_data == 1234);

        io_uring_support_free(ring);
    }

    SECTION("io_uring_support_sqe_submit_and_wait") {
        SECTION("valid ring") {
            io_uring_sqe_t *sqe;
            io_uring_cqe_t *cqe;
            uint32_t head;
            uint32_t count;

            io_uring_t *ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            sqe = io_uring_support_get_sqe(ring);
            REQUIRE(sqe != NULL);
            io_uring_prep_nop(sqe);
            sqe->user_data = 1234;

            REQUIRE(io_uring_support_sqe_submit_and_wait(ring, 1));

            count = 0;
            io_uring_for_each_cqe(ring, head, cqe) {
                REQUIRE(cqe->flags == 0);
                REQUIRE(cqe->res == 0);
                REQUIRE(cqe->user_data == 1234);
                count++;
            }
            REQUIRE(count == 1);

            io_uring_support_cq_advance(ring, count);

            io_uring_support_free(ring);
        }
    }

    SECTION("io_uring_support_sqe_enqueue_nop") {
        io_uring_t *ring;
        io_uring_cqe_t *cqe = NULL;

        ring = io_uring_support_init(10, NULL, NULL);

        REQUIRE(ring != NULL);

        REQUIRE(io_uring_support_sqe_enqueue_nop(
                ring,
                0,
                1234));

        io_uring_support_sqe_submit(ring);

        io_uring_wait_cqe(ring, &cqe);
        REQUIRE(cqe != NULL);
        REQUIRE(cqe->flags == 0);
        REQUIRE(cqe->res == 0);
        REQUIRE(cqe->user_data == 1234);
        io_uring_cqe_seen(ring, cqe);

        io_uring_support_free(ring);
    }

    SECTION("io_uring_support_sqe_enqueue_timeout") {
        SECTION("simple timer") {
            io_uring_t *ring;
            io_uring_cqe_t *cqe = NULL;

            struct __kernel_timespec ts = { 1, 0 };

            ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            REQUIRE(io_uring_support_sqe_enqueue_timeout(
                    ring,
                    0,
                    &ts,
                    0,
                    1234));

            io_uring_support_sqe_submit(ring);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res == -ETIME);
            REQUIRE(cqe->user_data == 1234);
            io_uring_cqe_seen(ring, cqe);

            io_uring_support_free(ring);
        }

        SECTION("no timeout if ops processed") {
            io_uring_t *ring;
            io_uring_cqe_t *cqe = NULL;
            uint32_t head, count;

            struct __kernel_timespec ts = { 1, 0 };

            ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            REQUIRE(io_uring_support_sqe_enqueue_timeout(
                    ring,
                    1,
                    &ts,
                    0,
                    1234));
            REQUIRE(io_uring_support_sqe_enqueue_nop(
                    ring,
                    0,
                    1234));

            io_uring_support_sqe_submit_and_wait(ring, 2);

            count = 0;
            io_uring_for_each_cqe(ring, head, cqe) {
                REQUIRE(cqe->flags == 0);
                REQUIRE(cqe->res == 0);
                REQUIRE(cqe->user_data == 1234);
                REQUIRE(count < 2);
                count++;
            }
            REQUIRE(count == 2);

            io_uring_support_cq_advance(ring, count);

            io_uring_support_free(ring);
        }
    }

    SECTION("io_uring_support_sqe_enqueue_accept") {
        uint16_t socket_port_free_ipv4 =
                network_tests_support_search_free_port_ipv4(9999);
        uint16_t socket_port_free_ipv6 =
                network_tests_support_search_free_port_ipv6(9999);

        SECTION("enqueue accept on valid socket ipv4") {
            io_uring_t *ring;
            io_uring_cqe_t *cqe = NULL;
            struct sockaddr_in server_address = {0};
            struct sockaddr_in client_address = {0};
            socklen_t client_address_len = 0;

            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(socket_port_free_ipv4);
            server_address.sin_addr.s_addr = loopback_ipv4.s_addr;

            int fd = network_io_common_socket_tcp4_new_server(
                    SOCK_NONBLOCK,
                    &server_address,
                    10,
                    NULL,
                    NULL);

            ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            REQUIRE(io_uring_support_sqe_enqueue_accept(
                    ring,
                    fd,
                    (sockaddr *)&client_address,
                    &client_address_len,
                    0,
                    0,
                    1234));

            io_uring_support_sqe_submit(ring);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res == -EAGAIN);
            REQUIRE(cqe->user_data == 1234);
            io_uring_cqe_seen(ring, cqe);

            io_uring_support_free(ring);

            REQUIRE(network_io_common_socket_close(fd, false));
        }

        SECTION("enqueue accept on invalid socket fd") {
            io_uring_t *ring;
            io_uring_cqe_t *cqe = NULL;
            struct sockaddr_in client_address = {0};
            socklen_t client_address_len = 0;

            ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            io_uring_support_sqe_enqueue_accept(
                    ring,
                    -1,
                    (sockaddr *)&client_address,
                    &client_address_len,
                    0,
                    0,
                    1234);

            io_uring_support_sqe_submit(ring);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res == -EBADF);
            REQUIRE(cqe->user_data == 1234);
            io_uring_cqe_seen(ring, cqe);

            io_uring_support_free(ring);
        }

        SECTION("enqueue accept and accept connection ipv4") {
            io_uring_t *ring;
            io_uring_cqe_t *cqe = NULL;
            int clientfd, serverfd, acceptedfd;
            struct sockaddr_in server_address = {0};
            struct sockaddr_in client_accept_address = {0};
            struct sockaddr_in client_connect_address = {0};
            socklen_t client_address_len = 0;

            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(socket_port_free_ipv4);
            server_address.sin_addr.s_addr = loopback_ipv4.s_addr;
            client_connect_address.sin_family = AF_INET;
            client_connect_address.sin_port = htons(socket_port_free_ipv4);
            client_connect_address.sin_addr.s_addr = loopback_ipv4.s_addr;

            clientfd = network_io_common_socket_tcp4_new(0);
            serverfd = network_io_common_socket_tcp4_new_server(
                    0,
                    &server_address,
                    10,
                    NULL,
                    NULL);

            ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            REQUIRE(io_uring_support_sqe_enqueue_accept(
                    ring,
                    serverfd,
                    (sockaddr *)&client_accept_address,
                    &client_address_len,
                    0,
                    0,
                    1234));

            // Submit first the sqe and then performs a blocking connection (shouldn't block unless there is a problem)
            io_uring_support_sqe_submit(ring);
            REQUIRE(connect(clientfd, (struct sockaddr*)&client_connect_address, sizeof(client_connect_address)) == 0);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res > 0);
            REQUIRE(cqe->user_data == 1234);
            REQUIRE(client_address_len == sizeof(client_accept_address));

            acceptedfd = cqe->res;
            io_uring_cqe_seen(ring, cqe);

            io_uring_support_free(ring);

            // Normally wouldn't really necessary to close both the accepted connection and the originating one because
            // we own both and are closing one end but let's just cover all the cases
            REQUIRE(network_io_common_socket_close(acceptedfd, false));
            REQUIRE(network_io_common_socket_close(clientfd, false));
            REQUIRE(network_io_common_socket_close(serverfd, false));
        }

        SECTION("enqueue accept fail too many sqe") {
            io_uring_t *ring;
            io_uring_cqe_t *cqe = NULL;
            struct sockaddr_in server_address = {0};
            struct sockaddr_in client_address = {0};
            socklen_t client_address_len = 0;
            ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            for(uint8_t i = 0; i < 16; i++) {
                REQUIRE(io_uring_support_get_sqe(ring) != NULL);
            }

            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(socket_port_free_ipv4);
            server_address.sin_addr.s_addr = loopback_ipv4.s_addr;

            int fd = network_io_common_socket_tcp4_new_server(
                    SOCK_NONBLOCK,
                    &server_address,
                    10,
                    NULL,
                    NULL);

            REQUIRE(!io_uring_support_sqe_enqueue_accept(
                    ring,
                    fd,
                    (sockaddr *)&client_address,
                    &client_address_len,
                    0,
                    0,
                    1234));

            io_uring_support_free(ring);

            REQUIRE(network_io_common_socket_close(fd, false));
        }
    }

    SECTION("io_uring_support_sqe_enqueue_recv") {
        uint16_t socket_port_free_ipv4 =
                network_tests_support_search_free_port_ipv4(9999);
        uint16_t socket_port_free_ipv6 =
                network_tests_support_search_free_port_ipv6(9999);

        SECTION("receive message") {
            io_uring_t *ring;
            io_uring_cqe_t *cqe;
            int clientfd, serverfd, acceptedfd;
            struct sockaddr_in server_address = {0};
            struct sockaddr_in client_accept_address = {0};
            struct sockaddr_in client_connect_address = {0};
            socklen_t client_address_len = 0;
            size_t buffer_send_data_len;
            char buffer_recv[64] = {0};
            char buffer_send[64] = {0};

            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(socket_port_free_ipv4);
            server_address.sin_addr.s_addr = loopback_ipv4.s_addr;
            client_connect_address.sin_family = AF_INET;
            client_connect_address.sin_port = htons(socket_port_free_ipv4);
            client_connect_address.sin_addr.s_addr = loopback_ipv4.s_addr;

            clientfd = network_io_common_socket_tcp4_new(0);
            serverfd = network_io_common_socket_tcp4_new_server(
                    0,
                    &server_address,
                    10,
                    NULL,
                    NULL);

            ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            REQUIRE(io_uring_support_sqe_enqueue_accept(
                    ring,
                    serverfd,
                    (sockaddr *)&client_accept_address,
                    &client_address_len,
                    0,
                    0,
                    1234));

            // Submit first the sqe and then performs a blocking connection (shouldn't block unless there is a problem)
            io_uring_support_sqe_submit(ring);
            REQUIRE(connect(clientfd, (struct sockaddr*)&client_connect_address, sizeof(client_connect_address)) == 0);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res > 0);
            REQUIRE(cqe->user_data == 1234);

            acceptedfd = cqe->res;
            io_uring_cqe_seen(ring, cqe);

            // Enqueue a recv sqe
            REQUIRE(io_uring_support_sqe_enqueue_recv(
                    ring,
                    acceptedfd,
                    &buffer_recv,
                    sizeof(buffer_recv),
                    0,
                    0,
                    4321));
            io_uring_support_sqe_submit(ring);

            // Send data from client fd and wait them on acceptedfd
            snprintf(buffer_send, 63, "RECV on io_uring");
            buffer_send_data_len = strlen(buffer_send) + 1;

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);

            cqe = NULL;
            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res == buffer_send_data_len);
            REQUIRE(cqe->user_data == 4321);
            REQUIRE(strncmp(buffer_recv, "RECV on io_uring", buffer_send_data_len) == 0);
            io_uring_cqe_seen(ring, cqe);

            io_uring_support_free(ring);

            // Normally wouldn't really necessary to close both the accepted connection and the originating one because
            // we own both and are closing one end but let's just cover all the cases
            REQUIRE(network_io_common_socket_close(acceptedfd, false));
            REQUIRE(network_io_common_socket_close(clientfd, false));
            REQUIRE(network_io_common_socket_close(serverfd, false));
        }

        SECTION("close socket") {
            io_uring_t *ring;
            io_uring_cqe_t *cqe = NULL;
            int clientfd, serverfd, acceptedfd;
            struct sockaddr_in server_address = {0};
            struct sockaddr_in client_accept_address = {0};
            struct sockaddr_in client_connect_address = {0};
            socklen_t client_address_len = 0;
            char buffer_recv[64] = {0};

            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(socket_port_free_ipv4);
            server_address.sin_addr.s_addr = loopback_ipv4.s_addr;
            client_connect_address.sin_family = AF_INET;
            client_connect_address.sin_port = htons(socket_port_free_ipv4);
            client_connect_address.sin_addr.s_addr = loopback_ipv4.s_addr;

            clientfd = network_io_common_socket_tcp4_new(0);
            serverfd = network_io_common_socket_tcp4_new_server(
                    0,
                    &server_address,
                    10,
                    NULL,
                    NULL);

            ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            REQUIRE(io_uring_support_sqe_enqueue_accept(
                    ring,
                    serverfd,
                    (sockaddr *)&client_accept_address,
                    &client_address_len,
                    0,
                    0,
                    1234));

            // Submit first the sqe and then performs a blocking connection (shouldn't block unless there is a problem)
            io_uring_support_sqe_submit(ring);
            REQUIRE(connect(clientfd, (struct sockaddr*)&client_connect_address, sizeof(client_connect_address)) == 0);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res > 0);
            REQUIRE(cqe->user_data == 1234);

            acceptedfd = cqe->res;
            io_uring_cqe_seen(ring, cqe);

            // Enqueue a recv sqe
            REQUIRE(io_uring_support_sqe_enqueue_recv(
                    ring,
                    acceptedfd,
                    &buffer_recv,
                    sizeof(buffer_recv),
                    0,
                    0,
                    4321));
            io_uring_support_sqe_submit(ring);

            REQUIRE(network_io_common_socket_close(clientfd, false));

            cqe = NULL;
            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res == 0);
            REQUIRE(cqe->user_data == 4321);
            io_uring_cqe_seen(ring, cqe);

            io_uring_support_free(ring);

            // Normally wouldn't really necessary to close both the accepted connection and the originating one because
            // we own both and are closing one end but let's just cover all the cases
            REQUIRE(network_io_common_socket_close(acceptedfd, false));
            REQUIRE(network_io_common_socket_close(serverfd, false));
        }

        SECTION("enqueue recv fail too many sqe") {
            io_uring_t *ring;
            io_uring_cqe_t *cqe = NULL;
            char buffer_recv[64] = {0};
            ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            for(uint8_t i = 0; i < 16; i++) {
                REQUIRE(io_uring_support_get_sqe(ring) != NULL);
            }

            int fd = network_io_common_socket_tcp4_new(
                    SOCK_NONBLOCK);

            REQUIRE(!io_uring_support_sqe_enqueue_recv(
                    ring,
                    fd,
                    &buffer_recv,
                    sizeof(buffer_recv),
                    0,
                    0,
                    4321));

            io_uring_support_free(ring);
        }
    }

    SECTION("io_uring_support_sqe_enqueue_send") {
        uint16_t socket_port_free_ipv4 =
                network_tests_support_search_free_port_ipv4(9999);
        uint16_t socket_port_free_ipv6 =
                network_tests_support_search_free_port_ipv6(9999);

        SECTION("send message") {
            io_uring_t *ring;
            io_uring_cqe_t *cqe = NULL;
            int clientfd, serverfd, acceptedfd;
            struct sockaddr_in server_address = {0};
            struct sockaddr_in client_accept_address = {0};
            struct sockaddr_in client_connect_address = {0};
            socklen_t client_address_len = 0;
            size_t buffer_send_data_len;
            char buffer_recv[64] = {0};
            char buffer_send[64] = {0};

            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(socket_port_free_ipv4);
            server_address.sin_addr.s_addr = loopback_ipv4.s_addr;
            client_connect_address.sin_family = AF_INET;
            client_connect_address.sin_port = htons(socket_port_free_ipv4);
            client_connect_address.sin_addr.s_addr = loopback_ipv4.s_addr;

            clientfd = network_io_common_socket_tcp4_new(0);
            serverfd = network_io_common_socket_tcp4_new_server(
                    0,
                    &server_address,
                    10,
                    NULL,
                    NULL);

            ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            REQUIRE(io_uring_support_sqe_enqueue_accept(
                    ring,
                    serverfd,
                    (sockaddr *)&client_accept_address,
                    &client_address_len,
                    0,
                    0,
                    1234));

            // Submit first the sqe and then performs a blocking connection (shouldn't block unless there is a problem)
            io_uring_support_sqe_submit(ring);
            REQUIRE(connect(clientfd, (struct sockaddr*)&client_connect_address, sizeof(client_connect_address)) == 0);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res > 0);
            REQUIRE(cqe->user_data == 1234);

            acceptedfd = cqe->res;
            io_uring_cqe_seen(ring, cqe);

            snprintf(buffer_send, 63, "SEND on io_uring");
            buffer_send_data_len = strlen(buffer_send) + 1;

            // Enqueue a send sqe
            REQUIRE(io_uring_support_sqe_enqueue_send(
                    ring,
                    acceptedfd,
                    &buffer_send,
                    buffer_send_data_len,
                    0,
                    0,
                    4321));
            io_uring_support_sqe_submit(ring);

            cqe = NULL;
            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res == buffer_send_data_len);
            REQUIRE(cqe->user_data == 4321);
            io_uring_cqe_seen(ring, cqe);

            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == buffer_send_data_len);

            REQUIRE(strncmp(buffer_recv, "SEND on io_uring", buffer_send_data_len) == 0);

            io_uring_support_free(ring);

            // Normally wouldn't really necessary to close both the accepted connection and the originating one because
            // we own both and are closing one end but let's just cover all the cases
            REQUIRE(network_io_common_socket_close(acceptedfd, false));
            REQUIRE(network_io_common_socket_close(clientfd, false));
            REQUIRE(network_io_common_socket_close(serverfd, false));
        }

        SECTION("enqueue send fail too many sqe") {
            io_uring_t *ring;
            io_uring_cqe_t *cqe = NULL;
            char buffer_recv[64] = {0};
            ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            for(uint8_t i = 0; i < 16; i++) {
                REQUIRE(io_uring_support_get_sqe(ring) != NULL);
            }

            int fd = network_io_common_socket_tcp4_new(
                    SOCK_NONBLOCK);

            REQUIRE(!io_uring_support_sqe_enqueue_send(
                    ring,
                    fd,
                    &buffer_recv,
                    sizeof(buffer_recv),
                    0,
                    0,
                    4321));

            io_uring_support_free(ring);
        }
    }

    SECTION("io_uring_support_sqe_enqueue_close") {
        uint16_t socket_port_free_ipv4 =
                network_tests_support_search_free_port_ipv4(9999);
        uint16_t socket_port_free_ipv6 =
                network_tests_support_search_free_port_ipv6(9999);

        SECTION("open and close socket") {
            io_uring_t *ring;
            io_uring_cqe_t *cqe = NULL;
            int clientfd, serverfd, acceptedfd;
            struct sockaddr_in server_address = {0};
            struct sockaddr_in client_accept_address = {0};
            struct sockaddr_in client_connect_address = {0};
            socklen_t client_address_len = 0;
            size_t buffer_send_data_len;
            char buffer_recv[64] = {0};
            char buffer_send[64] = {0};

            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(socket_port_free_ipv4);
            server_address.sin_addr.s_addr = loopback_ipv4.s_addr;
            client_connect_address.sin_family = AF_INET;
            client_connect_address.sin_port = htons(socket_port_free_ipv4);
            client_connect_address.sin_addr.s_addr = loopback_ipv4.s_addr;

            clientfd = network_io_common_socket_tcp4_new(0);
            serverfd = network_io_common_socket_tcp4_new_server(
                    0,
                    &server_address,
                    10,
                    NULL,
                    NULL);

            ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            REQUIRE(io_uring_support_sqe_enqueue_accept(
                    ring,
                    serverfd,
                    (sockaddr *)&client_accept_address,
                    &client_address_len,
                    0,
                    0,
                    1234));

            // Submit first the sqe and then performs a blocking connection (shouldn't block unless there is a problem)
            io_uring_support_sqe_submit(ring);
            REQUIRE(connect(clientfd, (struct sockaddr*)&client_connect_address, sizeof(client_connect_address)) == 0);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res > 0);
            REQUIRE(cqe->user_data == 1234);

            acceptedfd = cqe->res;
            io_uring_cqe_seen(ring, cqe);

            // Enqueue a close sqe
            REQUIRE(io_uring_support_sqe_enqueue_close(
                    ring,
                    acceptedfd,
                    0,
                    4321));
            io_uring_support_sqe_submit(ring);

            cqe = NULL;
            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->user_data == 4321);
            io_uring_cqe_seen(ring, cqe);

            io_uring_support_free(ring);

            // Normally wouldn't really necessary to close both the accepted connection and the originating one because
            // we own both and are closing one end but let's just cover all the cases
            REQUIRE(network_io_common_socket_close(clientfd, false));
            REQUIRE(network_io_common_socket_close(serverfd, false));
        }

        SECTION("enqueue close fail too many sqe") {
            io_uring_t *ring;
            io_uring_cqe_t *cqe;
            ring = io_uring_support_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            for(uint8_t i = 0; i < 16; i++) {
                REQUIRE(io_uring_support_get_sqe(ring) != NULL);
            }

            int fd = network_io_common_socket_tcp4_new(
                    SOCK_NONBLOCK);

            REQUIRE(!io_uring_support_sqe_enqueue_close(
                    ring,
                    fd,
                    0,
                    4321));

            io_uring_support_free(ring);
        }
    }

    SECTION("io_uring_support_sqe_enqueue_openat") {
        int fd = -1;
        io_uring_t *ring;
        io_uring_cqe_t *cqe;
        ring = io_uring_support_init(10, NULL, NULL);

        char fixture_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
        int fixture_temp_path_suffix_len = 4;
        close(mkstemps(fixture_temp_path, fixture_temp_path_suffix_len));

        SECTION("allocate sqe") {
            REQUIRE(io_uring_support_sqe_enqueue_openat(
                    ring,
                    0,
                    fixture_temp_path,
                    0,
                    0,
                    0,
                    0));
        }

        SECTION("open an existing file") {
            int flags = O_RDWR;

            REQUIRE(io_uring_support_sqe_enqueue_openat(
                    ring,
                    0,
                    fixture_temp_path,
                    flags,
                    0,
                    0,
                    4321));

            io_uring_support_sqe_submit(ring);

            cqe = NULL;
            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->user_data == 4321);
            REQUIRE(cqe->res > -1);

            fd = cqe->res;

            io_uring_cqe_seen(ring, cqe);
        }

        SECTION("open a non-existing file creating it") {
            int flags = O_CREAT | O_RDWR | O_EXCL;
            mode_t mode = S_IRUSR | S_IWUSR;

            // The file gets pre-created for convenience during the test setup, need to be unlinked for the test to
            // be able to reuse the unique file name
            unlink(fixture_temp_path);

            REQUIRE(io_uring_support_sqe_enqueue_openat(
                    ring,
                    0,
                    fixture_temp_path,
                    flags,
                    mode,
                    0,
                    4321));

            io_uring_support_sqe_submit(ring);

            cqe = NULL;
            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->user_data == 4321);
            REQUIRE(cqe->res > -1);

            fd = cqe->res;

            io_uring_cqe_seen(ring, cqe);
        }

        SECTION("fail to open an non-existing file without create option") {
            int flags = O_RDWR;

            // The file gets pre-created for convenience during the test setup, need to be unlinked for the test to
            // be able to reuse the unique file name
            unlink(fixture_temp_path);

            REQUIRE(io_uring_support_sqe_enqueue_openat(
                    ring,
                    0,
                    fixture_temp_path,
                    flags,
                    0,
                    0,
                    4321));

            io_uring_support_sqe_submit(ring);

            cqe = NULL;
            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->user_data == 4321);
            REQUIRE(cqe->res == -ENOENT);

            io_uring_cqe_seen(ring, cqe);
        }

        io_uring_support_free(ring);

        if (fd > -1) {
            close(fd);
        }

        unlink(fixture_temp_path);
    }

    SECTION("io_uring_support_sqe_enqueue_readv") {
        int fd = -1;
        char buffer_write[] = "cachegrand test - io_uring_support_sqe_enqueue_readv";
        char buffer_read1[128] = { 0 }, buffer_read2[128] = { 0 };
        struct iovec iovec[2] = { 0 };

        io_uring_t *ring;
        io_uring_cqe_t *cqe;
        ring = io_uring_support_init(10, NULL, NULL);

        char fixture_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
        int fixture_temp_path_suffix_len = 4;
        close(mkstemps(fixture_temp_path, fixture_temp_path_suffix_len));

        SECTION("allocate sqe") {
            REQUIRE(io_uring_support_sqe_enqueue_readv(
                    ring,
                    0,
                    NULL,
                    0,
                    0,
                    0,
                    0));
        }

        SECTION("read n. 1 iovec") {
            int flags = O_RDWR;

            iovec[0].iov_base = buffer_read1;
            iovec[0].iov_len = strlen(buffer_write);

            fd = openat(0, fixture_temp_path, flags, 0);
            REQUIRE(fd > -1);
            REQUIRE(write(fd, buffer_write, strlen(buffer_write)) == strlen(buffer_write));
            REQUIRE(lseek(fd, 0, SEEK_SET) == 0);

            REQUIRE(io_uring_support_sqe_enqueue_readv(
                    ring,
                    fd,
                    iovec,
                    1,
                    0,
                    0,
                    4321));

            io_uring_support_sqe_submit(ring);

            cqe = NULL;
            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->user_data == 4321);
            REQUIRE(cqe->res == strlen(buffer_write));
            REQUIRE(strncmp(buffer_write, buffer_read1, strlen(buffer_write)) == 0);

            io_uring_cqe_seen(ring, cqe);
        }

        SECTION("read n. 2 iovec") {
            int flags = O_RDWR;

            iovec[0].iov_base = buffer_read1;
            iovec[0].iov_len = strlen(buffer_write);
            iovec[1].iov_base = buffer_read2;
            iovec[1].iov_len = strlen(buffer_write);

            fd = openat(0, fixture_temp_path, flags, 0);
            REQUIRE(fd > -1);
            REQUIRE(write(fd, buffer_write, strlen(buffer_write)) == strlen(buffer_write));
            REQUIRE(write(fd, buffer_write, strlen(buffer_write)) == strlen(buffer_write));

            REQUIRE(io_uring_support_sqe_enqueue_readv(
                    ring,
                    fd,
                    iovec,
                    2,
                    0,
                    0,
                    4321));

            io_uring_support_sqe_submit(ring);

            cqe = NULL;
            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->user_data == 4321);
            REQUIRE(cqe->res == strlen(buffer_write) * 2);
            REQUIRE(strncmp(buffer_write, buffer_read1, strlen(buffer_write)) == 0);
            REQUIRE(strncmp(buffer_write, buffer_read2, strlen(buffer_write)) == 0);

            io_uring_cqe_seen(ring, cqe);
        }

        io_uring_support_free(ring);

        if (fd > -1) {
            close(fd);
        }

        unlink(fixture_temp_path);
    }

    SECTION("io_uring_support_sqe_enqueue_writev") {
        int fd = -1;
        char buffer_write[] = "cachegrand test - io_uring_support_sqe_enqueue_writev";
        char buffer_read1[128] = { 0 }, buffer_read2[128] = { 0 };
        struct iovec iovec[2] = { 0 };

        io_uring_t *ring;
        io_uring_cqe_t *cqe;
        ring = io_uring_support_init(10, NULL, NULL);

        char fixture_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
        int fixture_temp_path_suffix_len = 4;
        close(mkstemps(fixture_temp_path, fixture_temp_path_suffix_len));

        SECTION("allocate sqe") {
            REQUIRE(io_uring_support_sqe_enqueue_writev(
                    ring,
                    0,
                    NULL,
                    0,
                    0,
                    0,
                    0));
        }

        SECTION("write n. 1 iovec") {
            int flags = O_RDWR;

            iovec[0].iov_base = buffer_write;
            iovec[0].iov_len = strlen(buffer_write);

            fd = openat(0, fixture_temp_path, flags, 0);
            REQUIRE(fd > -1);

            REQUIRE(io_uring_support_sqe_enqueue_writev(
                    ring,
                    fd,
                    iovec,
                    1,
                    0,
                    0,
                    4321));

            io_uring_support_sqe_submit(ring);

            cqe = NULL;
            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->user_data == 4321);
            REQUIRE(cqe->res == strlen(buffer_write));

            REQUIRE(pread(fd, buffer_read1, strlen(buffer_write), 0) == strlen(buffer_write));
            REQUIRE(strncmp(buffer_write, buffer_read1, strlen(buffer_write)) == 0);

            io_uring_cqe_seen(ring, cqe);
        }

        SECTION("write n. 2 iovec") {
            int flags = O_RDWR;

            iovec[0].iov_base = buffer_write;
            iovec[0].iov_len = strlen(buffer_write);
            iovec[1].iov_base = buffer_write;
            iovec[1].iov_len = strlen(buffer_write);

            fd = openat(0, fixture_temp_path, flags, 0);
            REQUIRE(fd > -1);

            REQUIRE(io_uring_support_sqe_enqueue_writev(
                    ring,
                    fd,
                    iovec,
                    2,
                    0,
                    0,
                    4321));

            io_uring_support_sqe_submit(ring);

            cqe = NULL;
            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->user_data == 4321);
            REQUIRE(cqe->res == strlen(buffer_write) * 2);

            REQUIRE(pread(fd, buffer_read1, strlen(buffer_write), 0) == strlen(buffer_write));
            REQUIRE(pread(fd, buffer_read2, strlen(buffer_write), strlen(buffer_write)) == strlen(buffer_write));
            REQUIRE(strncmp(buffer_write, buffer_read1, strlen(buffer_write)) == 0);
            REQUIRE(strncmp(buffer_write, buffer_read2, strlen(buffer_write)) == 0);

            io_uring_cqe_seen(ring, cqe);
        }

        io_uring_support_free(ring);

        if (fd > -1) {
            close(fd);
        }

        unlink(fixture_temp_path);
    }

    SECTION("io_uring_support_sqe_enqueue_fsync") {
        int fd = -1;
        char buffer_write[] = "cachegrand test - io_uring_support_sqe_enqueue_fsync";

        io_uring_t *ring;
        io_uring_cqe_t *cqe;
        ring = io_uring_support_init(10, NULL, NULL);

        char fixture_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
        int fixture_temp_path_suffix_len = 4;
        close(mkstemps(fixture_temp_path, fixture_temp_path_suffix_len));

        SECTION("allocate sqe") {
            REQUIRE(io_uring_support_sqe_enqueue_fsync(
                    ring,
                    0,
                    0,
                    0,
                    0));
        }

        SECTION("write and flush") {
            int flags = O_RDWR;

            fd = openat(0, fixture_temp_path, flags, 0);
            REQUIRE(fd > -1);
            REQUIRE(write(fd, buffer_write, strlen(buffer_write)) == strlen(buffer_write));

            REQUIRE(io_uring_support_sqe_enqueue_fsync(
                    ring,
                    fd,
                    0,
                    0,
                    4321));

            io_uring_support_sqe_submit(ring);

            cqe = NULL;
            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->user_data == 4321);
            REQUIRE(cqe->res == 0);

            io_uring_cqe_seen(ring, cqe);
        }

        io_uring_support_free(ring);

        if (fd > -1) {
            close(fd);
        }

        unlink(fixture_temp_path);
    }

    SECTION("io_uring_support_sqe_enqueue_fallocate") {
        int fd = -1;
        char buffer_write[] = "cachegrand test - io_uring_support_sqe_enqueue_fallocate";

        io_uring_t *ring;
        io_uring_cqe_t *cqe;
        ring = io_uring_support_init(10, NULL, NULL);

        char fixture_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
        int fixture_temp_path_suffix_len = 4;
        close(mkstemps(fixture_temp_path, fixture_temp_path_suffix_len));

        SECTION("allocate sqe") {
            REQUIRE(io_uring_support_sqe_enqueue_fallocate(
                    ring,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0));
        }

        SECTION("create and extend to 1kb") {
            int flags = O_RDWR;
            struct stat statbuf = { 0 };
            mode_t mode = 0;
            off_t offset = 0;
            off_t len = 1024;

            fd = openat(0, fixture_temp_path, flags, 0);
            REQUIRE(fd > -1);

            REQUIRE(io_uring_support_sqe_enqueue_fallocate(
                    ring,
                    fd,
                    mode,
                    offset,
                    len,
                    0,
                    4321));

            io_uring_support_sqe_submit(ring);

            cqe = NULL;
            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->user_data == 4321);
            REQUIRE(cqe->res == 0);

            io_uring_cqe_seen(ring, cqe);

            REQUIRE(fstat(fd, &statbuf) == 0);
            REQUIRE(statbuf.st_size == 1024);
        }

        SECTION("create and extend to 1kb - no file size increase") {
            int flags = O_RDWR;
            struct stat statbuf = { 0 };
            mode_t mode = FALLOC_FL_KEEP_SIZE;
            off_t offset = 0;
            off_t len = 1024;

            fd = openat(0, fixture_temp_path, flags, 0);
            REQUIRE(fd > -1);

            REQUIRE(io_uring_support_sqe_enqueue_fallocate(
                    ring,
                    fd,
                    mode,
                    offset,
                    len,
                    0,
                    4321));

            io_uring_support_sqe_submit(ring);

            cqe = NULL;
            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe != NULL);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->user_data == 4321);
            REQUIRE(cqe->res == 0);

            io_uring_cqe_seen(ring, cqe);

            REQUIRE(fstat(fd, &statbuf) == 0);
            REQUIRE(statbuf.st_size == 0);
        }

        io_uring_support_free(ring);

        if (fd > -1) {
            close(fd);
        }

        unlink(fixture_temp_path);
    }
}
