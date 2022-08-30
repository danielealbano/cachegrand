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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - PEXPIREAT", "[redis][command][PEXPIREAT]") {
    char unixtime_ms_plus_5s_str[32] = {0 };
    int64_t unixtime_ms_plus_5s = clock_realtime_coarse_int64_ms() + 5000;
    snprintf(unixtime_ms_plus_5s_str, sizeof(unixtime_ms_plus_5s_str), "%ld", unixtime_ms_plus_5s);

    SECTION("No key") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str},
                ":0\r\n"));
    }

    SECTION("Existing key") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str},
                ":1\r\n"));
    }

    SECTION("Existing key - NX - key without expiry") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "NX"},
                ":1\r\n"));
    }

    SECTION("Existing key - NX - key with expiry") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value", "EX", "5"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "NX"},
                ":0\r\n"));
    }

    SECTION("Existing key - XX - key without expiry") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "XX"},
                ":0\r\n"));
    }

    SECTION("Existing key - XX - key with expiry") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value", "EX", "5"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "XX"},
                ":1\r\n"));
    }

    SECTION("Existing key - GT - key without expiry") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "GT"},
                ":0\r\n"));
    }

    SECTION("Existing key - GT - key with expiry - lower than") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value", "EX", "7"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "GT"},
                ":0\r\n"));
    }

    SECTION("Existing key - GT - key with expiry - greater than") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value", "EX", "3"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "GT"},
                ":1\r\n"));
    }

    SECTION("Existing key - LT - key without expiry") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "LT"},
                ":1\r\n"));
    }

    SECTION("Existing key - LT - key with expiry - lower than") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value", "EX", "7"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "LT"},
                ":1\r\n"));
    }

    SECTION("Existing key - LT - key with expiry - greater than") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value", "EX", "3"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "LT"},
                ":0\r\n"));
    }
}