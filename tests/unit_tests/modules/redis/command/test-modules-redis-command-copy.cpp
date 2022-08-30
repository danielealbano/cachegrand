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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - COPY", "[redis][command][COPY]") {
    SECTION("Non existent key") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"COPY", "a_key", "b_key"},
                ":0\r\n"));
    }

    SECTION("Existent key") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"COPY", "a_key", "b_key"},
                ":1\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET", "b_key"},
                "$7\r\nb_value\r\n"));
    }

    SECTION("Existent key - 4MB") {
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

        size_t expected_response_length = snprintf(
                nullptr,
                0,
                "$%lu\r\n%.*s\r\n",
                long_value_length,
                (int) long_value_length,
                long_value);

        char *expected_response = (char *) malloc(expected_response_length + 1);
        snprintf(
                expected_response,
                expected_response_length + 1,
                "$%lu\r\n%.*s\r\n",
                long_value_length,
                (int) long_value_length,
                long_value);

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", long_value},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"COPY", "a_key", "b_key"},
                ":1\r\n"));

        REQUIRE(send_recv_resp_command_multi_recv(
                client_fd,
                std::vector<std::string>{"GET", "a_key"},
                expected_response,
                expected_response_length,
                send_recv_resp_command_calculate_multi_recv(long_value_length)));

        REQUIRE(send_recv_resp_command_multi_recv(
                client_fd,
                std::vector<std::string>{"GET", "b_key"},
                expected_response,
                expected_response_length,
                send_recv_resp_command_calculate_multi_recv(long_value_length)));

        free(expected_response);
    }

    SECTION("Existent destination key - fail") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MSET", "a_key", "b_value", "b_key", "value_z"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"COPY", "a_key", "b_key"},
                ":0\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET", "b_key"},
                "$7\r\nvalue_z\r\n"));
    }

    SECTION("Existent destination key - Overwrite") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MSET", "a_key", "b_value", "b_key", "value_z"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"COPY", "a_key", "b_key", "REPLACE"},
                ":1\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET", "b_key"},
                "$7\r\nb_value\r\n"));
    }
}