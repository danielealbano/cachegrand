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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - GETRANGE", "[redis][command][GETRANGE]") {
    SECTION("Non-existing key") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET", "a_key"},
                "$-1\r\n"));
    }

    SECTION("Existing key - short value") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value"},
                "+OK\r\n"));

        SECTION("all") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", "0", "6"},
                    "$7\r\nb_value\r\n"));
        }

        SECTION("first char") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", "0", "0"},
                    "$1\r\nb\r\n"));
        }

        SECTION("last char") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", "6", "6"},
                    "$1\r\ne\r\n"));
        }

        SECTION("last char - negative start negative end") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", "-1", "-1"},
                    "$1\r\ne\r\n"));
        }

        SECTION("from the last fourth char to the second from the last") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", "-4", "-2"},
                    "$3\r\nalu\r\n"));
        }

        SECTION("end before start") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", "5", "3"},
                    "$0\r\n\r\n"));
        }

        SECTION("start after end") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", "7", "15"},
                    "$0\r\n\r\n"));
        }

        SECTION("start before end, length after end") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", "3", "15"},
                    "$4\r\nalue\r\n"));
        }
    }

    SECTION("Existing key - long value") {
        char *expected_response = nullptr;
        char *expected_response_static[256] = { nullptr };
        size_t long_value_length = 4 * 1024 * 1024;
        config_module_redis.max_command_length = long_value_length + 1024;

        // The long value is, on purpose, not filled with anything to have a very simple fuzzy testing (although
        // it's not repeatable)
        char *long_value = (char *) malloc(long_value_length + 1);

        // Fill with random data the long value
        char range = 'z' - 'a';
        for (size_t i = 0; i < long_value_length; i++) {
            long_value[i] = ((char)i % range) + 'a';
        }

        // This is legit as long_value_length + 1 is actually being allocated
        long_value[long_value_length] = 0;

        char end[32] = { 0 };
        snprintf(end, sizeof(end), "%lu", long_value_length - 1);

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", long_value},
                "+OK\r\n"));

        SECTION("all") {
            size_t expected_response_length = snprintf(
                    nullptr,
                    0,
                    "$%lu\r\n%.*s\r\n",
                    long_value_length,
                    (int) long_value_length,
                    long_value);

            expected_response = (char *) malloc(expected_response_length + 1);
            snprintf(
                    expected_response,
                    expected_response_length + 1,
                    "$%lu\r\n%.*s\r\n",
                    long_value_length,
                    (int) long_value_length,
                    long_value);

            REQUIRE(send_recv_resp_command_multi_recv(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", "0", end},
                    expected_response,
                    expected_response_length,
                    send_recv_resp_command_calculate_multi_recv(long_value_length)));
        }

        SECTION("first char") {
            snprintf((char*)expected_response_static, sizeof(expected_response_static), "$1\r\n%c\r\n", long_value[0]);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", "0", "0"},
                    (char*)expected_response_static));
        }

        SECTION("last char") {
            snprintf(
                    (char*)expected_response_static,
                    sizeof(expected_response_static),
                    "$1\r\n%c\r\n",
                    long_value[long_value_length - 1]);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", end, end},
                    (char*)expected_response_static));
        }

        SECTION("last char - negative start negative end") {
            snprintf(
                    (char*)expected_response_static,
                    sizeof(expected_response_static),
                    "$1\r\n%c\r\n",
                    long_value[long_value_length - 1]);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", "-1", "-1"},
                    (char*)expected_response_static));
        }

        SECTION("from the last fourth char to the second from the last") {
            snprintf(
                    (char*)expected_response_static,
                    sizeof(expected_response_static),
                    "$3\r\n%.*s\r\n",
                    3,
                    long_value + long_value_length - 4);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", "-4", "-2"},
                    (char*)expected_response_static));
        }

        SECTION("end before start") {
            char start1[32] = { 0 };
            snprintf(start1, sizeof(start1), "%lu", long_value_length - 50);
            char end1[32] = { 0 };
            snprintf(end1, sizeof(end1), "%lu", long_value_length - 100);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", start1, end1},
                    "$0\r\n\r\n"));
        }

        SECTION("start after end") {
            char start1[32] = { 0 };
            snprintf(start1, sizeof(start1), "%lu", long_value_length + 50);
            char end1[32] = { 0 };
            snprintf(end1, sizeof(end1), "%lu", long_value_length + 100);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", start1, end1},
                    "$0\r\n\r\n"));
        }

        SECTION("start before end, length after end") {
            char start1[32] = { 0 };
            snprintf(start1, sizeof(start1), "%lu", long_value_length -4);
            char end1[32] = { 0 };
            snprintf(end1, sizeof(end1), "%lu", long_value_length + 100);

            snprintf(
                    (char*)expected_response_static,
                    sizeof(expected_response_static),
                    "$4\r\n%s\r\n",
                    long_value + long_value_length - 4);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", start1, end1},
                    (char*)expected_response_static));
        }

        SECTION("50 chars after first chunk + 50") {
            off_t start1_int = ((64 * 1024) - 1) + 50;
            off_t end1_int = start1_int + 50;
            char start1[32] = { 0 };
            snprintf(start1, sizeof(start1), "%lu", start1_int);
            char end1[32] = { 0 };
            snprintf(end1, sizeof(end1), "%lu", end1_int);

            size_t expected_response_length = snprintf(
                    nullptr,
                    0,
                    "$%lu\r\n%.*s\r\n",
                    end1_int - start1_int + 1,
                    (int)(end1_int - start1_int + 1),
                    long_value + start1_int);

            expected_response = (char *)malloc(expected_response_length + 1);
            snprintf(
                    expected_response,
                    expected_response_length + 1,
                    "$%lu\r\n%.*s\r\n",
                    end1_int - start1_int + 1,
                    (int)(end1_int - start1_int + 1),
                    long_value + start1_int);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETRANGE", "a_key", start1, end1},
                    (char*)expected_response));
        }

        if (expected_response) {
            free(expected_response);
        }
    }

    SECTION("Missing parameters - key") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET"},
                "-ERR wrong number of arguments for 'GET' command\r\n"));
    }
}