/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <cassert>
#include <arpa/inet.h>
#include <linux/tls.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_internal.h>
#include <mbedtls/version.h>

#include "misc.h"
#include "exttypes.h"
#include "xalloc.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "config.h"
#include "support/simple_file_io.h"
#include "memory_allocator/ffma.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/network.h"

#include "network/network_tls.h"
#include "network/network_tls_mbedtls.h"

#include "../support.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

const unsigned char test_network_tls_certificate_pem[] = {
        0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x43,
        0x45, 0x52, 0x54, 0x49, 0x46, 0x49, 0x43, 0x41, 0x54, 0x45, 0x2d, 0x2d,
        0x2d, 0x2d, 0x2d, 0x0a, 0x4d, 0x49, 0x49, 0x43, 0x45, 0x6a, 0x43, 0x43,
        0x41, 0x58, 0x73, 0x43, 0x41, 0x67, 0x33, 0x36, 0x4d, 0x41, 0x30, 0x47,
        0x43, 0x53, 0x71, 0x47, 0x53, 0x49, 0x62, 0x33, 0x44, 0x51, 0x45, 0x42,
        0x42, 0x51, 0x55, 0x41, 0x4d, 0x49, 0x47, 0x62, 0x4d, 0x51, 0x73, 0x77,
        0x43, 0x51, 0x59, 0x44, 0x56, 0x51, 0x51, 0x47, 0x45, 0x77, 0x4a, 0x4b,
        0x55, 0x44, 0x45, 0x4f, 0x4d, 0x41, 0x77, 0x47, 0x0a, 0x41, 0x31, 0x55,
        0x45, 0x43, 0x42, 0x4d, 0x46, 0x56, 0x47, 0x39, 0x72, 0x65, 0x57, 0x38,
        0x78, 0x45, 0x44, 0x41, 0x4f, 0x42, 0x67, 0x4e, 0x56, 0x42, 0x41, 0x63,
        0x54, 0x42, 0x30, 0x4e, 0x6f, 0x64, 0x57, 0x38, 0x74, 0x61, 0x33, 0x55,
        0x78, 0x45, 0x54, 0x41, 0x50, 0x42, 0x67, 0x4e, 0x56, 0x42, 0x41, 0x6f,
        0x54, 0x43, 0x45, 0x5a, 0x79, 0x59, 0x57, 0x35, 0x72, 0x4e, 0x45, 0x52,
        0x45, 0x0a, 0x4d, 0x52, 0x67, 0x77, 0x46, 0x67, 0x59, 0x44, 0x56, 0x51,
        0x51, 0x4c, 0x45, 0x77, 0x39, 0x58, 0x5a, 0x57, 0x4a, 0x44, 0x5a, 0x58,
        0x4a, 0x30, 0x49, 0x46, 0x4e, 0x31, 0x63, 0x48, 0x42, 0x76, 0x63, 0x6e,
        0x51, 0x78, 0x47, 0x44, 0x41, 0x57, 0x42, 0x67, 0x4e, 0x56, 0x42, 0x41,
        0x4d, 0x54, 0x44, 0x30, 0x5a, 0x79, 0x59, 0x57, 0x35, 0x72, 0x4e, 0x45,
        0x52, 0x45, 0x49, 0x46, 0x64, 0x6c, 0x0a, 0x59, 0x69, 0x42, 0x44, 0x51,
        0x54, 0x45, 0x6a, 0x4d, 0x43, 0x45, 0x47, 0x43, 0x53, 0x71, 0x47, 0x53,
        0x49, 0x62, 0x33, 0x44, 0x51, 0x45, 0x4a, 0x41, 0x52, 0x59, 0x55, 0x63,
        0x33, 0x56, 0x77, 0x63, 0x47, 0x39, 0x79, 0x64, 0x45, 0x42, 0x6d, 0x63,
        0x6d, 0x46, 0x75, 0x61, 0x7a, 0x52, 0x6b, 0x5a, 0x43, 0x35, 0x6a, 0x62,
        0x32, 0x30, 0x77, 0x48, 0x68, 0x63, 0x4e, 0x4d, 0x54, 0x49, 0x77, 0x0a,
        0x4f, 0x44, 0x49, 0x79, 0x4d, 0x44, 0x55, 0x79, 0x4e, 0x6a, 0x55, 0x30,
        0x57, 0x68, 0x63, 0x4e, 0x4d, 0x54, 0x63, 0x77, 0x4f, 0x44, 0x49, 0x78,
        0x4d, 0x44, 0x55, 0x79, 0x4e, 0x6a, 0x55, 0x30, 0x57, 0x6a, 0x42, 0x4b,
        0x4d, 0x51, 0x73, 0x77, 0x43, 0x51, 0x59, 0x44, 0x56, 0x51, 0x51, 0x47,
        0x45, 0x77, 0x4a, 0x4b, 0x55, 0x44, 0x45, 0x4f, 0x4d, 0x41, 0x77, 0x47,
        0x41, 0x31, 0x55, 0x45, 0x0a, 0x43, 0x41, 0x77, 0x46, 0x56, 0x47, 0x39,
        0x72, 0x65, 0x57, 0x38, 0x78, 0x45, 0x54, 0x41, 0x50, 0x42, 0x67, 0x4e,
        0x56, 0x42, 0x41, 0x6f, 0x4d, 0x43, 0x45, 0x5a, 0x79, 0x59, 0x57, 0x35,
        0x72, 0x4e, 0x45, 0x52, 0x45, 0x4d, 0x52, 0x67, 0x77, 0x46, 0x67, 0x59,
        0x44, 0x56, 0x51, 0x51, 0x44, 0x44, 0x41, 0x39, 0x33, 0x64, 0x33, 0x63,
        0x75, 0x5a, 0x58, 0x68, 0x68, 0x62, 0x58, 0x42, 0x73, 0x0a, 0x5a, 0x53,
        0x35, 0x6a, 0x62, 0x32, 0x30, 0x77, 0x58, 0x44, 0x41, 0x4e, 0x42, 0x67,
        0x6b, 0x71, 0x68, 0x6b, 0x69, 0x47, 0x39, 0x77, 0x30, 0x42, 0x41, 0x51,
        0x45, 0x46, 0x41, 0x41, 0x4e, 0x4c, 0x41, 0x44, 0x42, 0x49, 0x41, 0x6b,
        0x45, 0x41, 0x6d, 0x2f, 0x78, 0x6d, 0x6b, 0x48, 0x6d, 0x45, 0x51, 0x72,
        0x75, 0x72, 0x45, 0x2f, 0x30, 0x72, 0x65, 0x2f, 0x6a, 0x65, 0x46, 0x52,
        0x4c, 0x6c, 0x0a, 0x38, 0x5a, 0x50, 0x6a, 0x42, 0x6f, 0x70, 0x37, 0x75,
        0x4c, 0x48, 0x68, 0x6e, 0x69, 0x61, 0x37, 0x6c, 0x51, 0x47, 0x2f, 0x35,
        0x7a, 0x44, 0x74, 0x5a, 0x49, 0x55, 0x43, 0x33, 0x52, 0x56, 0x70, 0x71,
        0x44, 0x53, 0x77, 0x42, 0x75, 0x77, 0x2f, 0x4e, 0x54, 0x77, 0x65, 0x47,
        0x79, 0x75, 0x50, 0x2b, 0x6f, 0x38, 0x41, 0x47, 0x39, 0x38, 0x48, 0x78,
        0x71, 0x78, 0x54, 0x42, 0x77, 0x49, 0x44, 0x0a, 0x41, 0x51, 0x41, 0x42,
        0x4d, 0x41, 0x30, 0x47, 0x43, 0x53, 0x71, 0x47, 0x53, 0x49, 0x62, 0x33,
        0x44, 0x51, 0x45, 0x42, 0x42, 0x51, 0x55, 0x41, 0x41, 0x34, 0x47, 0x42,
        0x41, 0x42, 0x53, 0x32, 0x54, 0x4c, 0x75, 0x42, 0x65, 0x54, 0x50, 0x6d,
        0x63, 0x61, 0x54, 0x61, 0x55, 0x57, 0x2f, 0x4c, 0x43, 0x42, 0x32, 0x4e,
        0x59, 0x4f, 0x79, 0x38, 0x47, 0x4d, 0x64, 0x7a, 0x52, 0x31, 0x6d, 0x78,
        0x0a, 0x38, 0x69, 0x42, 0x49, 0x75, 0x32, 0x48, 0x36, 0x2f, 0x45, 0x32,
        0x74, 0x69, 0x59, 0x33, 0x52, 0x49, 0x65, 0x76, 0x56, 0x32, 0x4f, 0x57,
        0x36, 0x31, 0x71, 0x59, 0x32, 0x2f, 0x58, 0x52, 0x51, 0x67, 0x37, 0x59,
        0x50, 0x78, 0x78, 0x33, 0x66, 0x66, 0x65, 0x55, 0x75, 0x67, 0x58, 0x39,
        0x46, 0x34, 0x4a, 0x2f, 0x69, 0x50, 0x6e, 0x6e, 0x75, 0x31, 0x7a, 0x41,
        0x78, 0x78, 0x79, 0x42, 0x79, 0x0a, 0x32, 0x56, 0x67, 0x75, 0x4b, 0x76,
        0x34, 0x53, 0x57, 0x6a, 0x52, 0x46, 0x6f, 0x52, 0x6b, 0x49, 0x66, 0x49,
        0x6c, 0x48, 0x58, 0x30, 0x71, 0x56, 0x76, 0x69, 0x4d, 0x68, 0x53, 0x6c,
        0x4e, 0x79, 0x32, 0x69, 0x6f, 0x46, 0x4c, 0x79, 0x37, 0x4a, 0x63, 0x50,
        0x5a, 0x62, 0x2b, 0x76, 0x33, 0x66, 0x74, 0x44, 0x47, 0x79, 0x77, 0x55,
        0x71, 0x63, 0x42, 0x69, 0x56, 0x44, 0x6f, 0x65, 0x61, 0x30, 0x0a, 0x48,
        0x6e, 0x2b, 0x47, 0x6d, 0x78, 0x5a, 0x41, 0x0a, 0x2d, 0x2d, 0x2d, 0x2d,
        0x2d, 0x45, 0x4e, 0x44, 0x20, 0x43, 0x45, 0x52, 0x54, 0x49, 0x46, 0x49,
        0x43, 0x41, 0x54, 0x45, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a
};
unsigned int test_network_tls_certificate_pem_len = 778;

const unsigned char test_network_tls_private_key_pem[] = {
        0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x52,
        0x53, 0x41, 0x20, 0x50, 0x52, 0x49, 0x56, 0x41, 0x54, 0x45, 0x20, 0x4b,
        0x45, 0x59, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a, 0x4d, 0x49, 0x49, 0x42,
        0x4f, 0x77, 0x49, 0x42, 0x41, 0x41, 0x4a, 0x42, 0x41, 0x4a, 0x76, 0x38,
        0x5a, 0x70, 0x42, 0x35, 0x68, 0x45, 0x4b, 0x37, 0x71, 0x78, 0x50, 0x39,
        0x4b, 0x33, 0x76, 0x34, 0x33, 0x68, 0x55, 0x53, 0x35, 0x66, 0x47, 0x54,
        0x34, 0x77, 0x61, 0x4b, 0x65, 0x37, 0x69, 0x78, 0x34, 0x5a, 0x34, 0x6d,
        0x75, 0x35, 0x55, 0x42, 0x76, 0x2b, 0x63, 0x77, 0x37, 0x57, 0x53, 0x46,
        0x0a, 0x41, 0x74, 0x30, 0x56, 0x61, 0x61, 0x67, 0x30, 0x73, 0x41, 0x62,
        0x73, 0x50, 0x7a, 0x55, 0x38, 0x48, 0x68, 0x73, 0x72, 0x6a, 0x2f, 0x71,
        0x50, 0x41, 0x42, 0x76, 0x66, 0x42, 0x38, 0x61, 0x73, 0x55, 0x77, 0x63,
        0x43, 0x41, 0x77, 0x45, 0x41, 0x41, 0x51, 0x4a, 0x41, 0x47, 0x30, 0x72,
        0x33, 0x65, 0x7a, 0x48, 0x33, 0x35, 0x57, 0x46, 0x47, 0x31, 0x74, 0x47,
        0x47, 0x61, 0x55, 0x4f, 0x72, 0x0a, 0x51, 0x41, 0x36, 0x31, 0x63, 0x79,
        0x61, 0x49, 0x49, 0x35, 0x33, 0x5a, 0x64, 0x67, 0x43, 0x52, 0x31, 0x49,
        0x55, 0x38, 0x62, 0x78, 0x37, 0x41, 0x55, 0x65, 0x76, 0x6d, 0x6b, 0x46,
        0x74, 0x42, 0x66, 0x2b, 0x61, 0x71, 0x4d, 0x57, 0x75, 0x73, 0x57, 0x56,
        0x4f, 0x57, 0x4a, 0x76, 0x47, 0x75, 0x32, 0x72, 0x35, 0x56, 0x70, 0x48,
        0x56, 0x41, 0x49, 0x6c, 0x38, 0x6e, 0x46, 0x36, 0x44, 0x53, 0x0a, 0x6b,
        0x51, 0x49, 0x68, 0x41, 0x4d, 0x6a, 0x45, 0x4a, 0x33, 0x7a, 0x56, 0x59,
        0x61, 0x32, 0x2f, 0x4d, 0x6f, 0x34, 0x65, 0x79, 0x2b, 0x69, 0x55, 0x39,
        0x4a, 0x39, 0x56, 0x64, 0x2b, 0x57, 0x6f, 0x79, 0x58, 0x44, 0x51, 0x44,
        0x34, 0x45, 0x45, 0x74, 0x77, 0x6d, 0x79, 0x47, 0x31, 0x50, 0x70, 0x41,
        0x69, 0x45, 0x41, 0x78, 0x75, 0x5a, 0x6c, 0x76, 0x68, 0x44, 0x49, 0x62,
        0x62, 0x63, 0x65, 0x0a, 0x37, 0x6f, 0x35, 0x42, 0x76, 0x4f, 0x68, 0x6e,
        0x43, 0x5a, 0x32, 0x4e, 0x37, 0x6b, 0x59, 0x62, 0x31, 0x5a, 0x43, 0x35,
        0x37, 0x67, 0x33, 0x46, 0x2b, 0x63, 0x62, 0x4a, 0x79, 0x57, 0x38, 0x43,
        0x49, 0x51, 0x43, 0x62, 0x73, 0x44, 0x47, 0x48, 0x42, 0x74, 0x6f, 0x32,
        0x71, 0x4a, 0x79, 0x46, 0x78, 0x62, 0x41, 0x4f, 0x37, 0x75, 0x51, 0x38,
        0x59, 0x30, 0x55, 0x56, 0x48, 0x61, 0x30, 0x4a, 0x0a, 0x42, 0x4f, 0x2f,
        0x67, 0x39, 0x30, 0x30, 0x53, 0x41, 0x63, 0x4a, 0x62, 0x63, 0x51, 0x49,
        0x67, 0x52, 0x74, 0x45, 0x6c, 0x6a, 0x49, 0x53, 0x68, 0x4f, 0x42, 0x38,
        0x70, 0x44, 0x6a, 0x72, 0x73, 0x51, 0x50, 0x78, 0x6d, 0x49, 0x31, 0x42,
        0x4c, 0x68, 0x6e, 0x6a, 0x44, 0x31, 0x45, 0x68, 0x52, 0x53, 0x75, 0x62,
        0x77, 0x68, 0x44, 0x77, 0x35, 0x41, 0x46, 0x55, 0x43, 0x49, 0x51, 0x43,
        0x4e, 0x0a, 0x41, 0x32, 0x34, 0x70, 0x44, 0x74, 0x64, 0x4f, 0x48, 0x79,
        0x64, 0x77, 0x74, 0x53, 0x42, 0x35, 0x2b, 0x7a, 0x46, 0x71, 0x46, 0x4c,
        0x66, 0x6d, 0x56, 0x5a, 0x70, 0x6c, 0x51, 0x4d, 0x2f, 0x67, 0x35, 0x6b,
        0x62, 0x34, 0x73, 0x6f, 0x37, 0x30, 0x59, 0x77, 0x3d, 0x3d, 0x0a, 0x2d,
        0x2d, 0x2d, 0x2d, 0x2d, 0x45, 0x4e, 0x44, 0x20, 0x52, 0x53, 0x41, 0x20,
        0x50, 0x52, 0x49, 0x56, 0x41, 0x54, 0x45, 0x20, 0x4b, 0x45, 0x59, 0x2d,
        0x2d, 0x2d, 0x2d, 0x2d, 0x0a
};
unsigned int test_network_tls_private_key_pem_len = 497;


TEST_CASE("network_tls.c", "[network][network_tls]") {
    SECTION("network_tls_is_ulp_tls_supported_internal") {
        char buffer[256] = { 0 };
        bool supported = false;

        if (simple_file_io_read(
                NETWORK_TLS_PROC_SYS_NET_IPV4_TCP_AVAILABLE_ULP,
                buffer,
                sizeof(buffer))) {
            if (strstr(buffer, "tls") != nullptr) {
                supported = true;
            }
        }

        REQUIRE(network_tls_is_ulp_tls_supported_internal() == supported);
    }

    SECTION("network_tls_is_ulp_tls_supported") {
        char buffer[256] = { 0 };
        bool supported = false;

        if (simple_file_io_read(
                NETWORK_TLS_PROC_SYS_NET_IPV4_TCP_AVAILABLE_ULP,
                buffer,
                sizeof(buffer))) {
            if (strstr(buffer, "tls") != nullptr) {
                supported = true;
            }
        }

        SECTION("first call") {
            REQUIRE(network_tls_is_ulp_tls_supported_internal() == supported);
        }

        SECTION("cached result") {
            REQUIRE(network_tls_is_ulp_tls_supported_internal() == supported);
        }
    }

    SECTION("network_tls_build_cipher_suites_from_names") {
        int *cipher_suites_ids = nullptr;
        size_t cipher_suites_ids_size = 0;

        SECTION("no names") {
            REQUIRE(network_tls_build_cipher_suites_from_names(
                    nullptr,
                    0,
                    &cipher_suites_ids_size) == nullptr);
            REQUIRE(cipher_suites_ids_size == 0);
        }

        SECTION("all valid names") {
            char *names[] = {
                    "TLS-ECDHE-RSA-WITH-AES-256-GCM-SHA384",
                    "TLS-DHE-RSA-WITH-AES-256-GCM-SHA384",
                    "TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256",
                    "TLS-DHE-RSA-WITH-AES-128-GCM-SHA256",
                    "TLS-DHE-RSA-WITH-AES-128-CCM",
                    "TLS-DHE-RSA-WITH-AES-128-CCM-8",
            };
            int names_count = sizeof(names) / sizeof(char*);

            REQUIRE((cipher_suites_ids = network_tls_build_cipher_suites_from_names(
                    names,
                    names_count,
                    &cipher_suites_ids_size)) != nullptr);

            REQUIRE(cipher_suites_ids_size == ((names_count + 1) * sizeof(int)));
            for(int index = 0; index < names_count; index++) {
                const mbedtls_ssl_ciphersuite_t *mbedtls_ssl_ciphersuite =
                        mbedtls_ssl_ciphersuite_from_string(names[index]);
                REQUIRE(mbedtls_ssl_ciphersuite->id == cipher_suites_ids[index]);
            }
        }

        SECTION("one incorrect name") {
            char *names[] = {
                    "TLS-ECDHE-RSA-WITH-AES-256-GCM-SHA384",
                    "TLS-DHE-RSA-WITH-AES-256-GCM-SHA384",
                    "TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256",
                    "TLS-DHE-RSA-WITH-AES-128-GCM-SHA256",
                    "TLS-DHE-RSA-WITH-AES-128-CCM",
                    "NON-EXISTING-CIPHER-SUITE",
            };
            int names_count = sizeof(names) / sizeof(char*);

            REQUIRE((cipher_suites_ids = network_tls_build_cipher_suites_from_names(
                    names,
                    names_count,
                    &cipher_suites_ids_size)) != nullptr);

            REQUIRE(cipher_suites_ids_size == (names_count) * sizeof(int));
            for(int index = 0; index < names_count - 1; index++) {
                const mbedtls_ssl_ciphersuite_t *mbedtls_ssl_ciphersuite =
                        mbedtls_ssl_ciphersuite_from_string(names[index]);
                REQUIRE(mbedtls_ssl_ciphersuite->id == cipher_suites_ids[index]);
            }
        }

        SECTION("all incorrect names") {
            char *names[] = {
                    "NON-EXISTING-CIPHER-SUITE-1",
                    "NON-EXISTING-CIPHER-SUITE-2",
                    "NON-EXISTING-CIPHER-SUITE-3",
            };
            int names_count = sizeof(names) / sizeof(char*);

            REQUIRE(network_tls_build_cipher_suites_from_names(
                    names,
                    names_count,
                    &cipher_suites_ids_size) == nullptr);
            REQUIRE(cipher_suites_ids_size == 0);
        }

        if (!cipher_suites_ids) {
            xalloc_free(cipher_suites_ids);
        }
    }

    SECTION("network_tls_does_ulp_tls_support_mbedtls_cipher_suite") {
        REQUIRE(network_tls_does_ulp_tls_support_mbedtls_cipher_suite(MBEDTLS_CIPHER_AES_128_GCM));
        REQUIRE(network_tls_does_ulp_tls_support_mbedtls_cipher_suite(MBEDTLS_CIPHER_AES_256_GCM));
        REQUIRE(network_tls_does_ulp_tls_support_mbedtls_cipher_suite(MBEDTLS_CIPHER_AES_128_CCM));
#if defined(TLS_CIPHER_CHACHA20_POLY1305)
        REQUIRE(network_tls_does_ulp_tls_support_mbedtls_cipher_suite(MBEDTLS_CIPHER_CHACHA20_POLY1305));
#endif
    }

    SECTION("network_tls_min_version_config_to_mbed") {
        REQUIRE(network_tls_min_version_config_to_mbed(
                CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_ANY) == MBEDTLS_SSL_MINOR_VERSION_1);
        REQUIRE(network_tls_min_version_config_to_mbed(
                CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_0) == MBEDTLS_SSL_MINOR_VERSION_1);
        REQUIRE(network_tls_min_version_config_to_mbed(
                CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_1) == MBEDTLS_SSL_MINOR_VERSION_2);
        REQUIRE(network_tls_min_version_config_to_mbed(
                CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_2) == MBEDTLS_SSL_MINOR_VERSION_3);
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
        REQUIRE(network_tls_min_version_config_to_mbed(
                CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_3) == MBEDTLS_SSL_MINOR_VERSION_4);
#endif
    }

    SECTION("network_tls_max_version_config_to_mbed") {
        REQUIRE(network_tls_max_version_config_to_mbed(
                CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_0) == MBEDTLS_SSL_MINOR_VERSION_1);
        REQUIRE(network_tls_max_version_config_to_mbed(
                CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_1) == MBEDTLS_SSL_MINOR_VERSION_2);
        REQUIRE(network_tls_max_version_config_to_mbed(
                CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_2) == MBEDTLS_SSL_MINOR_VERSION_3);
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
        REQUIRE(network_tls_max_version_config_to_mbed(
                CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_3) == MBEDTLS_SSL_MINOR_VERSION_4);
        REQUIRE(network_tls_max_version_config_to_mbed(
                CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_ANY) == MBEDTLS_SSL_MINOR_VERSION_4);
#else
        REQUIRE(network_tls_max_version_config_to_mbed(
                CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_ANY) == MBEDTLS_SSL_MINOR_VERSION_3);
#endif
    }

    SECTION("network_tls_load_certificate") {
        bool res;
        mbedtls_x509_crt certificate;
        mbedtls_x509_crt_init(&certificate);

        SECTION("valid") {
            TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                    (const char*)test_network_tls_certificate_pem,
                    test_network_tls_certificate_pem_len,
                    certificate_path,
                    {
                        res = network_tls_load_certificate(&certificate, certificate_path);
                    });

            REQUIRE(res);
        }

        SECTION("valid") {
            TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                    "test",
                    4,
                    certificate_path,
                    {
                        res = network_tls_load_certificate(&certificate, certificate_path);
                    });

            REQUIRE(!res);
        }

        mbedtls_x509_crt_free(&certificate);
    }

    SECTION("network_tls_load_private_key") {
        bool res;
        mbedtls_pk_context private_key;
        mbedtls_pk_init(&private_key);

        SECTION("valid") {
            TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                    (const char*)test_network_tls_private_key_pem,
                    test_network_tls_private_key_pem_len,
                    certificate_path,
                    {
                        res = network_tls_load_private_key(&private_key, certificate_path);
                    });

            REQUIRE(res);
        }

        SECTION("valid") {
            TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                    "test",
                    4,
                    certificate_path,
                    {
                        res = network_tls_load_private_key(&private_key, certificate_path);
                    });

            REQUIRE(!res);
        }

        mbedtls_pk_free(&private_key);
    }

    SECTION("network_tls_config_init") {
        network_tls_config_t *network_tls_config;

        SECTION("valid") {
            TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                    (const char*)test_network_tls_certificate_pem,
                    test_network_tls_certificate_pem_len,
                    certificate_path,
                    {
                        TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                                (const char*)test_network_tls_private_key_pem,
                                test_network_tls_private_key_pem_len,
                                private_key_path,
                                {
                                        network_tls_config = network_tls_config_init(
                                                certificate_path,
                                                private_key_path,
                                                nullptr,
                                                CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_ANY,
                                                CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_ANY,
                                                nullptr,
                                                0,
                                                false);
                                });
                    });

            REQUIRE(network_tls_config);
        }

        SECTION("valid - enforce min tls version") {
            TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                    (const char*)test_network_tls_certificate_pem,
                    test_network_tls_certificate_pem_len,
                    certificate_path,
                    {
                        TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                                (const char*)test_network_tls_private_key_pem,
                                test_network_tls_private_key_pem_len,
                                private_key_path,
                                {
                                        network_tls_config = network_tls_config_init(
                                                certificate_path,
                                                private_key_path,
                                                nullptr,
                                                CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_2,
                                                CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_2,
                                                nullptr,
                                                0,
                                                false);
                                });
                    });

            REQUIRE(network_tls_config);
        }

        SECTION("valid - enforce max tls version") {
            TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                    (const char*)test_network_tls_certificate_pem,
                    test_network_tls_certificate_pem_len,
                    certificate_path,
                    {
                        TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                                (const char*)test_network_tls_private_key_pem,
                                test_network_tls_private_key_pem_len,
                                private_key_path,
                                {
                                        network_tls_config = network_tls_config_init(
                                                certificate_path,
                                                private_key_path,
                                                nullptr,
                                                CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_ANY,
                                                CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_2,
                                                nullptr,
                                                0,
                                                false);
                                });
                    });

            REQUIRE(network_tls_config);
        }

        SECTION("valid - enforce cipher suites") {
            char *names[] = {
                    "TLS-ECDHE-RSA-WITH-AES-256-GCM-SHA384",
                    "TLS-DHE-RSA-WITH-AES-256-GCM-SHA384",
                    "TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256",
                    "TLS-DHE-RSA-WITH-AES-128-GCM-SHA256",
                    "TLS-DHE-RSA-WITH-AES-128-CCM",
                    "TLS-DHE-RSA-WITH-AES-128-CCM-8",
            };
            int names_count = sizeof(names) / sizeof(char*);

            size_t cipher_suites_ids_size;
            int *cipher_suites_ids = network_tls_build_cipher_suites_from_names(
                    names,
                    names_count,
                    &cipher_suites_ids_size);

            TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                    (const char*)test_network_tls_certificate_pem,
                    test_network_tls_certificate_pem_len,
                    certificate_path,
                    {
                        TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                                (const char*)test_network_tls_private_key_pem,
                                test_network_tls_private_key_pem_len,
                                private_key_path,
                                {
                                        network_tls_config = network_tls_config_init(
                                                certificate_path,
                                                private_key_path,
                                                nullptr,
                                                CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_ANY,
                                                CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_2,
                                                cipher_suites_ids,
                                                cipher_suites_ids_size,
                                                false);
                                });
                    });

            REQUIRE(network_tls_config);

            xalloc_free(cipher_suites_ids);
        }

        SECTION("invalid certificate file") {
            TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                    "test",
                    4,
                    certificate_path,
                    {
                        TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                                (const char*)test_network_tls_private_key_pem,
                                test_network_tls_private_key_pem_len,
                                private_key_path,
                                {
                                        network_tls_config = network_tls_config_init(
                                                certificate_path,
                                                private_key_path,
                                                nullptr,
                                                CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_ANY,
                                                CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_ANY,
                                                nullptr,
                                                0,
                                                false);
                                });
                    });

            REQUIRE(network_tls_config == nullptr);
        }

        SECTION("invalid private key file") {
            TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                    (const char*)test_network_tls_certificate_pem,
                    test_network_tls_certificate_pem_len,
                    certificate_path,
                    {
                        TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                                "test",
                                4,
                                private_key_path,
                                {
                                        network_tls_config = network_tls_config_init(
                                                certificate_path,
                                                private_key_path,
                                                nullptr,
                                                CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_ANY,
                                                CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_ANY,
                                                nullptr,
                                                0,
                                                false);
                                });
                    });

            REQUIRE(network_tls_config == nullptr);
        }

        if (network_tls_config) {
            mbedtls_pk_free(&network_tls_config->server_key);
            mbedtls_x509_crt_free(&network_tls_config->server_cert);
            mbedtls_ctr_drbg_free(&network_tls_config->ctr_drbg);
            mbedtls_entropy_free(&network_tls_config->entropy);
            mbedtls_ssl_config_free(&network_tls_config->config);

            xalloc_free(network_tls_config);
        }
    }

    SECTION("network_tls_config_free") {
        SECTION("non null") {
            network_tls_config_t *network_tls_config;

            TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                    (const char*)test_network_tls_certificate_pem,
                    test_network_tls_certificate_pem_len,
                    certificate_path,
                    {
                        TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                                (const char*)test_network_tls_private_key_pem,
                                test_network_tls_private_key_pem_len,
                                private_key_path,
                                {
                                    network_tls_config = network_tls_config_init(
                                            certificate_path,
                                            private_key_path,
                                            nullptr,
                                            CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_ANY,
                                            CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_ANY,
                                            nullptr,
                                            0,
                                            false);
                                });
                    });

            network_tls_config_free(network_tls_config);
        }

        SECTION("null") {
            // Should not crash
            network_tls_config_free(nullptr);
        }
    }

    SECTION("network_tls_mbedtls_version") {
        REQUIRE(strcmp(network_tls_mbedtls_version(), MBEDTLS_VERSION_STRING_FULL) == 0);
    }

    SECTION("network_tls_mbedtls_get_all_cipher_suites_info") {
        const int *mbedtls_cipher_suites_ids = mbedtls_ssl_list_ciphersuites();

        // Count the cipher suites supported by mbedtls that can be used
        int count = 0;
        const mbedtls_ssl_ciphersuite_t *mbedtls_ssl_ciphersuite_first = nullptr;
        for(
                const int *mbedtls_cipher_suite_id = mbedtls_cipher_suites_ids;
                *mbedtls_cipher_suite_id;
                mbedtls_cipher_suite_id++) {
            const mbedtls_ssl_ciphersuite_t *mbedtls_ssl_ciphersuite =
                    mbedtls_ssl_ciphersuite_from_id(*mbedtls_cipher_suite_id);

            // Skip all the non-tls ciphers
            if (
                    mbedtls_ssl_ciphersuite->max_major_ver < 3 ||
                    (mbedtls_ssl_ciphersuite->max_major_ver == 3 && mbedtls_ssl_ciphersuite->max_minor_ver == 0)) {
                continue;
            }

            if (!mbedtls_ssl_ciphersuite_first) {
                mbedtls_ssl_ciphersuite_first = mbedtls_ssl_ciphersuite;
            }

            count++;
        }

        network_tls_mbedtls_cipher_suite_info_t *cipher_suites_info =
                network_tls_mbedtls_get_all_cipher_suites_info();

        // The list of cipher suites always end with a nullptr
        REQUIRE(cipher_suites_info[count].name == nullptr);

        // Validate the first cipher suite as spot-checking
        REQUIRE(cipher_suites_info[0].name == mbedtls_ssl_ciphersuite_first->name);

        // If min/max_minor_ver is greater than zero the value will match min/max_version
        if (mbedtls_ssl_ciphersuite_first->min_minor_ver > 0) {
            REQUIRE(cipher_suites_info[0].min_version == mbedtls_ssl_ciphersuite_first->min_minor_ver);
        }
        if (mbedtls_ssl_ciphersuite_first->max_minor_ver > 0) {
            REQUIRE(cipher_suites_info[0].max_version == mbedtls_ssl_ciphersuite_first->max_minor_ver);
        }

        REQUIRE(cipher_suites_info[0].offloading ==
            network_tls_does_ulp_tls_support_mbedtls_cipher_suite(mbedtls_ssl_ciphersuite_first->cipher));

        xalloc_free(cipher_suites_info);
    }

    SECTION("network_tls_min_version_to_string") {
        char *versions[] = { "any","TLS 1.0","TLS 1.1","TLS 1.2","TLS 1.3" };

        SECTION("any") {
            REQUIRE(strcmp(
                    network_tls_min_version_to_string(CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_ANY),
                    versions[CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_ANY]) == 0);
        }

        SECTION("TLS 1.0") {
            REQUIRE(strcmp(
                    network_tls_min_version_to_string(CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_0),
                    versions[CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_0]) == 0);
        }

        SECTION("TLS 1.1") {
            REQUIRE(strcmp(
                    network_tls_min_version_to_string(CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_1),
                    versions[CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_1]) == 0);
        }

        SECTION("TLS 1.2") {
            REQUIRE(strcmp(
                    network_tls_min_version_to_string(CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_2),
                    versions[CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_2]) == 0);
        }

        SECTION("TLS 1.3") {
            REQUIRE(strcmp(
                    network_tls_min_version_to_string(CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_3),
                    versions[CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_3]) == 0);
        }
    }

    SECTION("network_tls_max_version_to_string") {
        char *versions[] = { "any","TLS 1.0","TLS 1.1","TLS 1.2","TLS 1.3" };

        SECTION("any") {
            REQUIRE(strcmp(
                    network_tls_max_version_to_string(CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_ANY),
                    versions[CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_ANY]) == 0);
        }

        SECTION("TLS 1.0") {
            REQUIRE(strcmp(
                    network_tls_max_version_to_string(CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_0),
                    versions[CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_0]) == 0);
        }

        SECTION("TLS 1.1") {
            REQUIRE(strcmp(
                    network_tls_max_version_to_string(CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_1),
                    versions[CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_1]) == 0);
        }

        SECTION("TLS 1.2") {
            REQUIRE(strcmp(
                    network_tls_max_version_to_string(CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_2),
                    versions[CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_2]) == 0);
        }

        SECTION("TLS 1.3") {
            REQUIRE(strcmp(
                    network_tls_max_version_to_string(CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_3),
                    versions[CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_3]) == 0);
        }
    }
}
