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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - KEYS", "[redis][command][KEYS]") {
    SECTION("Empty database") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"KEYS", "nomatch"},
                "*0\r\n"));
    }

    SECTION("One key") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MSET", "a_key", "b_value"},
                "+OK\r\n"));

        SECTION("No match") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"KEYS", "nomatch"},
                    "*0\r\n"));
        }

        SECTION("Match - simple") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"KEYS", "a_key"},
                    "*1\r\n$5\r\na_key\r\n"));
        }

        SECTION("Match - star") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"KEYS", "a_*"},
                    "*1\r\n$5\r\na_key\r\n"));
        }

        SECTION("Match - question mark") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"KEYS", "a_???"},
                    "*1\r\n$5\r\na_key\r\n"));
        }

        SECTION("Match - backslash") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"KEYS", "a\_key"},
                    "*1\r\n$5\r\na_key\r\n"));
        }

        SECTION("Match - brackets") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"KEYS", "[a-z]_key"},
                    "*1\r\n$5\r\na_key\r\n"));
        }

        SECTION("Match - everything") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"KEYS", "*"},
                    "*1\r\n$5\r\na_key\r\n"));
        }
    }

    SECTION("Multiple keys") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{
                        "MSET",
                        "a_key", "a_value",
                        "b_key", "b_value",
                        "c_key", "c_value",
                        "d_key", "d_value",
                        "key_zzz", "value_z"},
                "+OK\r\n"));

        SECTION("No match") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"KEYS", "nomatch"},
                    "*0\r\n"));
        }

        SECTION("Match - simple") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"KEYS", "a_key"},
                    "*1\r\n$5\r\na_key\r\n"));
        }

        SECTION("Match - star - 1 result") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"KEYS", "a_*"},
                    "*1\r\n$5\r\na_key\r\n"));
        }

        SECTION("Match - star - multiple results") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"KEYS", "*key"},
                    "*4\r\n$5\r\nb_key\r\n$5\r\na_key\r\n$5\r\nd_key\r\n$5\r\nc_key\r\n"));
        }

        SECTION("Match - question mark") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"KEYS", "a_???"},
                    "*1\r\n$5\r\na_key\r\n"));
        }

        SECTION("Match - backslash") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"KEYS", "a\_key"},
                    "*1\r\n$5\r\na_key\r\n"));
        }

        SECTION("Match - brackets") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"KEYS", "[a-z]_key"},
                    "*4\r\n$5\r\nb_key\r\n$5\r\na_key\r\n$5\r\nd_key\r\n$5\r\nc_key\r\n"));
        }

        SECTION("Match - everything") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"KEYS", "*"},
                    "*5\r\n$7\r\nkey_zzz\r\n$5\r\nb_key\r\n$5\r\na_key\r\n$5\r\nd_key\r\n$5\r\nc_key\r\n"));
        }
    }
}