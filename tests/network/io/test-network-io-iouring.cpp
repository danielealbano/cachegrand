#include "../../catch.hpp"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#include <liburing.h>

#include "network/io/network_io_common.h"
#include "network/io/network_io_iouring.h"

#include "network_io_tests_support.h"

TEST_CASE("network/io/network_io_uring", "[network][network_io][network_io_uring]") {
    struct in_addr loopback_ipv4 = {0};
    struct in_addr loopback_ipv6 = {0};
    uint16_t socket_port_free_ipv4 =
            network_io_tests_support_search_free_port_ipv4(9999);
    uint16_t socket_port_free_ipv6 =
            network_io_tests_support_search_free_port_ipv6(9999);

    inet_pton(AF_INET, "127.0.0.1", &loopback_ipv4);
    inet_pton(AF_INET, "::1", &loopback_ipv6);

    SECTION("network_io_iouring_init") {
        SECTION("null params") {
            io_uring_t *ring = network_io_iouring_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            network_io_iouring_free(ring);
        }

        SECTION("with params") {
            io_uring_params_t params = {0};
            io_uring_t *ring = network_io_iouring_init(10, &params, NULL);

            REQUIRE(ring != NULL);
            REQUIRE(params.features != 0);

            network_io_iouring_free(ring);
        }
    }

    SECTION("network_io_iouring_probe_feature") {
        SECTION("existing feature") {
            REQUIRE(network_io_iouring_probe_feature(0x01 | 0x02 | 0x08, 0x01));
        }

        SECTION("non-existing feature") {
            REQUIRE(!network_io_iouring_probe_feature(0x01 | 0x02 | 0x08, 0x04));
        }
    }

    SECTION("network_io_iouring_probe_opcode") {
        SECTION("valid opcode") {
            io_uring_t *ring = network_io_iouring_init(10, NULL, NULL);

            REQUIRE(ring != NULL);
            REQUIRE(network_io_iouring_probe_opcode(ring, IORING_OP_READV));

            network_io_iouring_free(ring);
        }

        SECTION("invalid opcode") {
            io_uring_t *ring = network_io_iouring_init(10, NULL, NULL);

            REQUIRE(ring != NULL);
            REQUIRE(!network_io_iouring_probe_opcode(ring, IORING_OP_LAST));

            network_io_iouring_free(ring);
        }
    }

    SECTION("network_io_iouring_get_sqe") {
        SECTION("fetch one") {
            io_uring_t *ring = network_io_iouring_init(10, NULL, NULL);

            REQUIRE(ring != NULL);
            REQUIRE(network_io_iouring_get_sqe(ring) != NULL);

            network_io_iouring_free(ring);
        }

        SECTION("overflow") {
            io_uring_t *ring = network_io_iouring_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            for(uint8_t i = 0; i < 16; i++) {
                REQUIRE(network_io_iouring_get_sqe(ring) != NULL);
            }
            REQUIRE(network_io_iouring_get_sqe(ring) == NULL);

            network_io_iouring_free(ring);
        }
    }

    SECTION("network_io_iouring_sqe_submit") {
        io_uring_sqe_t *sqe;
        io_uring_cqe_t *cqe;

        io_uring_t *ring = network_io_iouring_init(10, NULL, NULL);

        sqe = network_io_iouring_get_sqe(ring);
        REQUIRE(sqe != NULL);
        io_uring_prep_nop(sqe);
        sqe->user_data = 1234;

        network_io_iouring_sqe_submit(ring);

        io_uring_wait_cqe(ring, &cqe);
        REQUIRE(cqe->flags == 0);
        REQUIRE(cqe->res == 0);
        REQUIRE(cqe->user_data == 1234);

        network_io_iouring_free(ring);
    }

    SECTION("network_io_iouring_sqe_submit_and_wait") {
        io_uring_sqe_t *sqe;
        io_uring_cqe_t *cqe;
        uint32_t head;
        uint32_t count;

        io_uring_t *ring = network_io_iouring_init(10, NULL, NULL);

        sqe = network_io_iouring_get_sqe(ring);
        REQUIRE(sqe != NULL);
        io_uring_prep_nop(sqe);
        sqe->user_data = 1234;

        network_io_iouring_sqe_submit_and_wait(ring, 1);

        count = 0;
        network_io_iouring_cqe_foreach(ring, head, cqe) {
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res == 0);
            REQUIRE(cqe->user_data == 1234);
            count++;
        }
        REQUIRE(count == 1);

        network_io_iouring_cq_advance(ring, count);

        network_io_iouring_free(ring);
    }

    SECTION("network_io_iouring_sqe_enqueue_accept") {
        SECTION("enqueue accept on valid socket ipv4") {
            io_uring_cqe_t *cqe;
            struct sockaddr_in server_address = {0};
            struct sockaddr_in client_address = {0};
            socklen_t client_address_len = 0;

            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(socket_port_free_ipv4);
            server_address.sin_addr.s_addr = loopback_ipv4.s_addr;

            int fd = network_io_common_socket_tcp4_new_server(
                    SOCK_NONBLOCK,
                    &server_address,
                    10);

            io_uring_t *ring = network_io_iouring_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            REQUIRE(network_io_iouring_sqe_enqueue_accept(
                    ring,
                    fd,
                    (sockaddr *)&client_address,
                    &client_address_len,
                    0,
                    1234));

            network_io_iouring_sqe_submit_and_wait(ring, 1);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res == -EAGAIN);
            REQUIRE(cqe->user_data == 1234);
            io_uring_cqe_seen(ring, cqe);

            network_io_iouring_free(ring);

            network_io_common_socket_close(fd, false);
        }

        SECTION("enqueue accept on invalid socket fd") {
            io_uring_cqe_t *cqe;
            struct sockaddr_in client_address = {0};
            socklen_t client_address_len = 0;

            io_uring_t *ring = network_io_iouring_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            network_io_iouring_sqe_enqueue_accept(
                    ring,
                    -1,
                    (sockaddr *)&client_address,
                    &client_address_len,
                    0,
                    1234);

            network_io_iouring_sqe_submit_and_wait(ring, 1);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res == -EBADF);
            REQUIRE(cqe->user_data == 1234);
            io_uring_cqe_seen(ring, cqe);

            network_io_iouring_free(ring);
        }

        SECTION("enqueue accept and accept connection ipv4") {
            int clientfd, serverfd, acceptedfd;
            io_uring_cqe_t *cqe;
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
                    10);

            io_uring_t *ring = network_io_iouring_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            REQUIRE(network_io_iouring_sqe_enqueue_accept(
                    ring,
                    serverfd,
                    (sockaddr *)&client_accept_address,
                    &client_address_len,
                    0,
                    1234));

            // Submit first the sqe and then performs a blocking connection (shouldn't block unless there is a problem)
            network_io_iouring_sqe_submit(ring);
            REQUIRE(connect(clientfd, (struct sockaddr*)&client_connect_address, sizeof(client_connect_address)) == 0);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res > 0);
            REQUIRE(cqe->user_data == 1234);
            REQUIRE(client_address_len == sizeof(client_accept_address));
            REQUIRE(client_accept_address.sin_addr.s_addr == client_connect_address.sin_addr.s_addr);

            acceptedfd = cqe->res;
            io_uring_cqe_seen(ring, cqe);

            network_io_iouring_free(ring);

            // Normally wouldn't really necessary to close both the accepted connection and the originating one because
            // we own both and are closing one end but let's just cover all the cases
            network_io_common_socket_close(acceptedfd, false);
            network_io_common_socket_close(clientfd, false);
            network_io_common_socket_close(serverfd, false);
        }
    }

    SECTION("network_io_iouring_sqe_enqueue_recv") {
        SECTION("receive message") {
            int clientfd, serverfd, acceptedfd;
            io_uring_cqe_t *cqe;
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
                    10);

            io_uring_t *ring = network_io_iouring_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            REQUIRE(network_io_iouring_sqe_enqueue_accept(
                    ring,
                    serverfd,
                    (sockaddr *)&client_accept_address,
                    &client_address_len,
                    0,
                    1234));

            // Submit first the sqe and then performs a blocking connection (shouldn't block unless there is a problem)
            network_io_iouring_sqe_submit(ring);
            REQUIRE(connect(clientfd, (struct sockaddr*)&client_connect_address, sizeof(client_connect_address)) == 0);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res > 0);
            REQUIRE(cqe->user_data == 1234);

            acceptedfd = cqe->res;
            io_uring_cqe_seen(ring, cqe);

            // Enqueue a recv sqe
            REQUIRE(network_io_iouring_sqe_enqueue_recv(
                    ring,
                    acceptedfd,
                    &buffer_recv,
                    sizeof(buffer_recv),
                    4321));
            network_io_iouring_sqe_submit(ring);

            // Send data from client fd and wait them on acceptedfd
            snprintf(buffer_send, 63, "RECV on io_uring");
            buffer_send_data_len = strlen(buffer_send) + 1;

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res == buffer_send_data_len);
            REQUIRE(cqe->user_data == 4321);
            REQUIRE(strncmp(buffer_recv, "RECV on io_uring", buffer_send_data_len) == 0);
            io_uring_cqe_seen(ring, cqe);

            network_io_iouring_free(ring);

            // Normally wouldn't really necessary to close both the accepted connection and the originating one because
            // we own both and are closing one end but let's just cover all the cases
            network_io_common_socket_close(acceptedfd, false);
            network_io_common_socket_close(clientfd, false);
            network_io_common_socket_close(serverfd, false);
        }

        SECTION("close socket") {
            int clientfd, serverfd, acceptedfd;
            io_uring_cqe_t *cqe;
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
                    10);

            io_uring_t *ring = network_io_iouring_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            REQUIRE(network_io_iouring_sqe_enqueue_accept(
                    ring,
                    serverfd,
                    (sockaddr *)&client_accept_address,
                    &client_address_len,
                    0,
                    1234));

            // Submit first the sqe and then performs a blocking connection (shouldn't block unless there is a problem)
            network_io_iouring_sqe_submit(ring);
            REQUIRE(connect(clientfd, (struct sockaddr*)&client_connect_address, sizeof(client_connect_address)) == 0);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res > 0);
            REQUIRE(cqe->user_data == 1234);

            acceptedfd = cqe->res;
            io_uring_cqe_seen(ring, cqe);

            // Enqueue a recv sqe
            REQUIRE(network_io_iouring_sqe_enqueue_recv(
                    ring,
                    acceptedfd,
                    &buffer_recv,
                    sizeof(buffer_recv),
                    4321));
            network_io_iouring_sqe_submit(ring);

            network_io_common_socket_close(clientfd, false);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res == 0);
            REQUIRE(cqe->user_data == 4321);
            io_uring_cqe_seen(ring, cqe);

            network_io_iouring_free(ring);

            // Normally wouldn't really necessary to close both the accepted connection and the originating one because
            // we own both and are closing one end but let's just cover all the cases
            network_io_common_socket_close(acceptedfd, false);
            network_io_common_socket_close(serverfd, false);
        }
    }

    SECTION("network_io_iouring_sqe_enqueue_send") {
        SECTION("send message") {
            int clientfd, serverfd, acceptedfd;
            io_uring_cqe_t *cqe;
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
                    10);

            io_uring_t *ring = network_io_iouring_init(10, NULL, NULL);

            REQUIRE(ring != NULL);

            REQUIRE(network_io_iouring_sqe_enqueue_accept(
                    ring,
                    serverfd,
                    (sockaddr *)&client_accept_address,
                    &client_address_len,
                    0,
                    1234));

            // Submit first the sqe and then performs a blocking connection (shouldn't block unless there is a problem)
            network_io_iouring_sqe_submit(ring);
            REQUIRE(connect(clientfd, (struct sockaddr*)&client_connect_address, sizeof(client_connect_address)) == 0);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res > 0);
            REQUIRE(cqe->user_data == 1234);

            acceptedfd = cqe->res;
            io_uring_cqe_seen(ring, cqe);

            snprintf(buffer_send, 63, "SEND on io_uring");
            buffer_send_data_len = strlen(buffer_send) + 1;

            // Enqueue a send sqe
            REQUIRE(network_io_iouring_sqe_enqueue_send(
                    ring,
                    acceptedfd,
                    &buffer_send,
                    buffer_send_data_len,
                    4321));
            network_io_iouring_sqe_submit(ring);

            io_uring_wait_cqe(ring, &cqe);
            REQUIRE(cqe->flags == 0);
            REQUIRE(cqe->res == buffer_send_data_len);
            REQUIRE(cqe->user_data == 4321);
            io_uring_cqe_seen(ring, cqe);

            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == buffer_send_data_len);

            REQUIRE(strncmp(buffer_recv, "SEND on io_uring", buffer_send_data_len) == 0);

            network_io_iouring_free(ring);

            // Normally wouldn't really necessary to close both the accepted connection and the originating one because
            // we own both and are closing one end but let's just cover all the cases
            network_io_common_socket_close(acceptedfd, false);
            network_io_common_socket_close(clientfd, false);
            network_io_common_socket_close(serverfd, false);
        }
    }
}
