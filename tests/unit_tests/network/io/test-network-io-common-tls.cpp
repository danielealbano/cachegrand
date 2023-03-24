/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <linux/tls.h>

#include "config.h"
#include "protocol/redis/protocol_redis.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/io/network_io_common_tls.h"
#include "network/network_tls.h"

#include "../network_tests_support.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

TEST_CASE("network/io/network_io_common_tls.c", "[network][network_io][network_io_common_tls]") {
    struct sockaddr_in listener_address = {0};
    struct sockaddr_in client_address = {0};
    struct in_addr loopback_ipv4 = { 0 };
    int listener_fd, server_fd, client_fd;

    inet_pton(AF_INET, "127.0.0.1", &loopback_ipv4);
    uint16_t socket_port_free_ipv4 = network_tests_support_search_free_port_ipv4();

    listener_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    listener_address.sin_family = AF_INET;
    listener_address.sin_port = htons(socket_port_free_ipv4);
    listener_address.sin_addr.s_addr = loopback_ipv4.s_addr;

    REQUIRE(listener_fd > 0);
    REQUIRE(network_io_common_socket_set_linger(listener_fd, false, 0));
    REQUIRE(network_io_common_socket_set_reuse_port(listener_fd, true));
    REQUIRE(network_io_common_socket_bind(
            listener_fd,
            (struct sockaddr*)&listener_address, sizeof(listener_address)));
    REQUIRE(network_io_common_socket_listen(listener_fd, 10));

    client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    REQUIRE(network_io_common_socket_set_linger(client_fd, true, 0));
    client_address.sin_family = AF_INET;
    client_address.sin_port = htons(socket_port_free_ipv4);
    client_address.sin_addr.s_addr = loopback_ipv4.s_addr;
    REQUIRE(connect(client_fd, (sockaddr*)&client_address, sizeof(client_address)) != -1);

    server_fd = accept(listener_fd, nullptr, nullptr);
    REQUIRE(network_io_common_socket_set_linger(server_fd, true, 0));

    SECTION("network_io_common_tls_socket_set_ulp") {
        if (network_tls_is_ulp_tls_supported()) {
            SECTION("valid option") {
                REQUIRE(network_io_common_tls_socket_set_ulp(server_fd, "tls"));
            }

            SECTION("invalid option") {
                REQUIRE(network_io_common_tls_socket_set_ulp(server_fd, "non existant ulp") == false);
            }
        } else {
            WARN("Can't test kTLS, tls ulp not available, try to load the tls kernel module");
        }
    }

    SECTION("network_io_common_tls_socket_set_tls_rx") {
        if (network_tls_is_ulp_tls_supported()) {
            REQUIRE(network_io_common_tls_socket_set_ulp(server_fd, "tls"));

            SECTION("valid option") {
                tls12_crypto_info_aes_gcm_128_t tls12_crypto_info_aes_gcm_128 = {
                        .info = {
                                .version = TLS_1_2_VERSION,
                                .cipher_type = TLS_CIPHER_AES_GCM_128,
                        },
                        .iv = { 0 },
                        .key = { 0 },
                        .salt = { 0 },
                        .rec_seq = { 0 },
                };

                REQUIRE(network_io_common_tls_socket_set_tls_rx(
                        server_fd,
                        (network_io_common_tls_crypto_info_t*)&tls12_crypto_info_aes_gcm_128,
                        sizeof(tls12_crypto_info_aes_gcm_128)));
            }

            SECTION("invalid option") {
                REQUIRE(network_io_common_tls_socket_set_tls_rx(server_fd, nullptr, 0) == false);
            }
        } else {
            WARN("Can't test kTLS, tls ulp not available, try to load the tls kernel module");
        }
    }

    SECTION("network_io_common_tls_socket_set_tls_tx") {
        if (network_tls_is_ulp_tls_supported()) {
            REQUIRE(network_io_common_tls_socket_set_ulp(server_fd, "tls"));

            SECTION("valid option") {
                tls12_crypto_info_aes_gcm_128_t tls12_crypto_info_aes_gcm_128 = {
                        .info = {
                                .version = TLS_1_2_VERSION,
                                .cipher_type = TLS_CIPHER_AES_GCM_128,
                        },
                        .iv = { 0 },
                        .key = { 0 },
                        .salt = { 0 },
                        .rec_seq = { 0 },
                };

                REQUIRE(network_io_common_tls_socket_set_tls_tx(
                        server_fd,
                        (network_io_common_tls_crypto_info_t*)&tls12_crypto_info_aes_gcm_128,
                        sizeof(tls12_crypto_info_aes_gcm_128)));
            }

            SECTION("invalid option") {
                REQUIRE(network_io_common_tls_socket_set_tls_tx(server_fd, nullptr, 0) == false);
            }
        } else {
            WARN("Can't test kTLS, tls ulp not available, try to load the tls kernel module");
        }
    }

    close(server_fd);
    close(client_fd);
    close(listener_fd);
}
