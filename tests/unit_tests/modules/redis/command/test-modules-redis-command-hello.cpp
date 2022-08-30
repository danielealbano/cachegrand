/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include <cstdbool>
#include <cstring>
#include <memory>
#include <string>

#include <netinet/in.h>

#include "clock.h"
#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "config.h"
#include "fiber.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "signal_handler_thread.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"

#include "program.h"

#include "test-modules-redis-command-fixture.hpp"

#pragma GCC diagnostic ignored "-Wwrite-strings"

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - HELLO", "[redis][command][HELLO]") {
    char *hello_v2_expected_response_start =
            "*14\r\n"
            "$6\r\nserver\r\n"
            "$17\r\ncachegrand-server\r\n"
            "$7\r\nversion\r\n"
            "$";
    char *hello_v2_expected_response_end =
            "\r\n"
            "$5\r\nproto\r\n"
            ":2\r\n"
            "$2\r\nid\r\n"
            ":0\r\n"
            "$4\r\nmode\r\n"
            "$10\r\nstandalone\r\n"
            "$4\r\nrole\r\n"
            "$6\r\nmaster\r\n"
            "$7\r\nmodules\r\n"
            "*0\r\n";

    char *hello_v3_expected_response_start =
            "%7\r\n"
            "$6\r\nserver\r\n$17\r\ncachegrand-server\r\n"
            "$7\r\nversion\r\n$";
    char *hello_v3_expected_response_end =
            "\r\n"
            "$5\r\nproto\r\n:3\r\n"
            "$2\r\nid\r\n:0\r\n"
            "$4\r\nmode\r\n$10\r\nstandalone\r\n"
            "$4\r\nrole\r\n$6\r\nmaster\r\n"
            "$7\r\nmodules\r\n*0\r\n";

    SECTION("HELLO - no version") {
        snprintf(buffer_send, sizeof(buffer_send) - 1, "*1\r\n$5\r\nHELLO\r\n");
        buffer_send_data_len = strlen(buffer_send);

        REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
        size_t len = recv(client_fd, buffer_recv, sizeof(buffer_recv), 0);

        REQUIRE(len > strlen(hello_v2_expected_response_start) + strlen(hello_v2_expected_response_end));

        REQUIRE(strncmp(buffer_recv, hello_v2_expected_response_start, strlen(hello_v2_expected_response_start)) == 0);
        REQUIRE(strncmp(
                buffer_recv + (len - strlen(hello_v2_expected_response_end)),
                hello_v2_expected_response_end,
                strlen(hello_v2_expected_response_end)) == 0);
    }

    SECTION("HELLO - v2") {
        snprintf(buffer_send, sizeof(buffer_send) - 1, "*2\r\n$5\r\nHELLO\r\n$1\r\n2\r\n");
        buffer_send_data_len = strlen(buffer_send);

        REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
        size_t len = recv(client_fd, buffer_recv, sizeof(buffer_recv), 0);

        REQUIRE(len > strlen(hello_v2_expected_response_start) + strlen(hello_v2_expected_response_end));

        REQUIRE(strncmp(buffer_recv, hello_v2_expected_response_start, strlen(hello_v2_expected_response_start)) == 0);
        REQUIRE(strncmp(
                buffer_recv + (len - strlen(hello_v2_expected_response_end)),
                hello_v2_expected_response_end,
                strlen(hello_v2_expected_response_end)) == 0);
    }

    SECTION("HELLO - v3") {
        snprintf(buffer_send, sizeof(buffer_send) - 1, "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n");
        buffer_send_data_len = strlen(buffer_send);

        REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
        size_t len = recv(client_fd, buffer_recv, sizeof(buffer_recv), 0);

        REQUIRE(len > strlen(hello_v3_expected_response_start) + strlen(hello_v3_expected_response_end));

        REQUIRE(strncmp(buffer_recv, hello_v3_expected_response_start, strlen(hello_v3_expected_response_start)) == 0);
        REQUIRE(strncmp(
                buffer_recv + (len - strlen(hello_v3_expected_response_end)),
                hello_v3_expected_response_end,
                strlen(hello_v3_expected_response_end)) == 0);
    }
}