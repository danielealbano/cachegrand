/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include <cstdbool>
#include <memory>

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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - LCS", "[redis][command][LCS]") {
    SECTION("Missing keys - String") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"LCS", "a_key", "b_key"},
                "$0\r\n\r\n"));
    }

    SECTION("Missing keys - Length") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"LCS", "a_key", "b_key", "LEN"},
                ":0\r\n"));
    }

    SECTION("Empty keys - String") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MSET", "a_key", "", "b_key", ""},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"LCS", "a_key", "b_key"},
                "$0\r\n\r\n"));
    }

    SECTION("Empty keys - Length") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MSET", "a_key", "", "b_key", ""},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"LCS", "a_key", "b_key", "LEN"},
                ":0\r\n"));
    }

    SECTION("No common substrings - String") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MSET", "a_key", "qwertyuiop", "b_key", "asdfghjkl"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"LCS", "a_key", "b_key"},
                "$0\r\n\r\n"));
    }

    SECTION("No common substrings - Length") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MSET", "a_key", "qwertyuiop", "b_key", "asdfghjkl"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"LCS", "a_key", "b_key", "LEN"},
                ":0\r\n"));
    }

    SECTION("One common substring - String") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MSET", "a_key", "b_value", "b_key", "value_z"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"LCS", "a_key", "b_key"},
                "$5\r\nvalue\r\n"));
    }

    SECTION("One common substring - String") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MSET", "a_key", "b_value", "b_key", "value_z"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"LCS", "a_key", "b_key", "LEN"},
                ":5\r\n"));
    }

    SECTION("Multiple common substring - String") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{
                        "MSET",
                        "a_key",
                        "a very long string",
                        "b_key",
                        "another very string but long"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"LCS", "a_key", "b_key"},
                "$13\r\na very string\r\n"));
    }

    SECTION("Multiple common substring - Length") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{
                        "MSET",
                        "a_key",
                        "a very long string",
                        "b_key",
                        "another very string but long"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"LCS", "a_key", "b_key", "LEN"},
                ":13\r\n"));
    }
}