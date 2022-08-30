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
#include <sys/types.h>

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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - MGET", "[redis][command][MGET]") {
    SECTION("Existing key") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MGET", "a_key"},
                "*1\r\n$7\r\nb_value\r\n"));
    }

    SECTION("Existing key - pipelining") {
        char buffer_recv_expected[512] = { 0 };
        char *buffer_send_start = buffer_send;
        char *buffer_recv_expected_start = buffer_recv_expected;

        snprintf(buffer_send, sizeof(buffer_send) - 1, "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$7\r\nb_value\r\n");
        buffer_send_data_len = strlen(buffer_send);

        REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
        REQUIRE(recv(client_fd, buffer_recv, sizeof(buffer_recv), 0) == 5);
        REQUIRE(strncmp(buffer_recv, "+OK\r\n", strlen("+OK\r\n")) == 0);

        for(int index = 0; index < 10; index++) {
            buffer_send_start += snprintf(
                    buffer_send_start,
                    sizeof(buffer_send) - (buffer_send_start - buffer_send) - 1,
                    "*2\r\n$4\r\nMGET\r\n$5\r\na_key\r\n");

            buffer_recv_expected_start += snprintf(
                    buffer_recv_expected_start,
                    sizeof(buffer_recv_expected) - (buffer_recv_expected_start - buffer_recv_expected) - 1,
                    "$7\r\nb_value\r\n");
        }
        buffer_send_data_len = strlen(buffer_send);

        REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);

        size_t recv_len = 0;
        do {
            recv_len += recv(client_fd, buffer_recv, sizeof(buffer_recv), 0);
        } while(recv_len < 130);

        REQUIRE(strncmp(buffer_recv, buffer_recv_expected_start, strlen(buffer_recv_expected_start)) == 0);
    }

    SECTION("Non-existing key") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MGET", "a_key"},
                "*1\r\n$-1\r\n"));
    }

    SECTION("Missing parameters - key") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MGET"},
                "-ERR wrong number of arguments for 'MGET' command\r\n"));
    }

    SECTION("Fetch 2 keys") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key_1", "b_value_1"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key_2", "b_value_2"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MGET", "a_key_1", "a_key_2"},
                "*2\r\n$9\r\nb_value_1\r\n$9\r\nb_value_2\r\n"));
    }

    SECTION("Fetch 128 keys") {
        int key_count = 128;
        std::vector<std::string> arguments = std::vector<std::string>();
        char buffer_recv_cmp[8 * 1024] = { 0 };
        size_t buffer_recv_cmp_length;
        off_t buffer_recv_cmp_offset = 0;
        off_t buffer_send_offset = 0;

        for(int key_index = 0; key_index < key_count; key_index++) {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{
                            "SET",
                            string_format("a_key_%05d", key_index),
                            string_format("b_value_%05d", key_index)
                    },
                    "+OK\r\n"));
        }

        buffer_recv_cmp_offset += snprintf(
                buffer_recv_cmp + buffer_recv_cmp_offset,
                sizeof(buffer_recv_cmp) - buffer_recv_cmp_offset - 1,
                "*128\r\n");

        arguments.emplace_back("MGET");
        for(int key_index = 0; key_index < key_count; key_index++) {
            arguments.push_back(string_format("a_key_%05d", key_index));

            buffer_recv_cmp_offset += snprintf(
                    buffer_recv_cmp + buffer_recv_cmp_offset,
                    sizeof(buffer_recv_cmp) - buffer_recv_cmp_offset - 1,
                    "$13\r\nb_value_%05d\r\n",
                    key_index);
        }

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                arguments,
                buffer_recv_cmp));
    }
}