#include "catch.hpp"

#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <arpa/inet.h>
#include <liburing/io_uring.h>

#include "log.h"

#include "network/io/network_io_common.h"
#include "network/io/network_io_iouring.h"

/*
 * Functions still to test
 *
void network_io_iouring_sqe_enqueue_accept(
        io_uring_t *ring,
        int fd,
        struct sockaddr *socket_address,
        socklen_t *socket_address_size,
        unsigned flags,
        uint64_t user_data);
void network_io_iouring_sqe_enqueue_recv(
        io_uring_t *ring,
        int fd,
        void *buffer,
        size_t buffer_size,
        uint64_t user_data);
void network_io_iouring_sqe_enqueue_send(
        io_uring_t *ring,
        int fd,
        void *buffer,
        size_t buffer_size,
        uint64_t user_data);
bool network_io_iouring_sqe_submit(
        io_uring_t *ring);
bool network_io_iouring_sqe_submit_and_wait(
        io_uring_t *ring,
        int wait_nr);
*/

TEST_CASE("network/io/network_io_uring", "[network][network_io][network_io_uring]") {
    struct in_addr loopback_ipv4 = {0};
    inet_pton(AF_INET, "127.0.0.1", &loopback_ipv4);

    SECTION("network_io_iouring_init") {
        SECTION("null params") {
            io_uring_t *ring = network_io_iouring_init(10, NULL, 0);

            REQUIRE(ring != NULL);

            network_io_iouring_free(ring);
        }

        SECTION("with params") {
            io_uring_params_t params = {0};
            io_uring_t *ring = network_io_iouring_init(10, &params, 0);

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
            io_uring_t *ring = network_io_iouring_init(10, NULL, 0);

            REQUIRE(network_io_iouring_probe_opcode(ring, IORING_OP_READV));

            network_io_iouring_free(ring);
        }

        SECTION("invalid opcode") {
            io_uring_t *ring = network_io_iouring_init(10, NULL, 0);

            REQUIRE(!network_io_iouring_probe_opcode(ring, IORING_OP_LAST));

            network_io_iouring_free(ring);
        }
    }

    SECTION("network_io_iouring_sqe_enqueue_accept") {
        SECTION("enqueue accept on valid socket") {
            struct sockaddr_in address = {0};
            address.sin_family = AF_INET;
            address.sin_port = htons(9999);
            address.sin_addr.s_addr = loopback_ipv4.s_addr;

            int fd = network_io_common_socket_tcp4_new_server(&address, 10);

            io_uring_t *ring = network_io_iouring_init(10, NULL, NULL);

            //network_io_iouring_sqe_enqueue_accept()

            network_io_iouring_free(ring);
        }

        SECTION("enqueue accept on invalid socket") {
            //network_io_iouring_sqe_enqueue_accept()
        }
    }
}
