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

#include <unistd.h>
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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - generic tests", "[redis][command][generic]") {
    SECTION("Unknown / unsupported command") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"UNKNOWN COMMAND"},
                "-ERR unknown command `UNKNOWN COMMAND` with `0` args\r\n"));
    }

    SECTION("Malformed - more data than declared") {
        snprintf(buffer_send, sizeof(buffer_send) - 1, "*1\r\n$5\r\nUNKNOWN COMMAND\r\n");
        buffer_send_data_len = strlen(buffer_send);

        REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
        REQUIRE(recv(client_fd, buffer_recv, sizeof(buffer_recv), 0) == 24);
        REQUIRE(strncmp(buffer_recv, "-ERR parsing error '8'\r\n",
                        strlen("-ERR parsing error '8'\r\n")) == 0);
    }

    SECTION("Timeout") {
        config_module_network_timeout.read_ms = 1000;

        // Send a NOP command to pick up the new timeout
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET", "a_value"},
                "$-1\r\n"));

        // Wait the read timeout plus 100ms
        usleep((config.modules[0].network->timeout->read_ms * 1000) + (100 * 1000));

        // The socket should be closed so recv should return 0
        REQUIRE(recv(client_fd, buffer_recv, sizeof(buffer_recv), 0) == 0);
    }

    SECTION("Command too long") {
        int cmd_length = (int)config_module_redis.max_command_length + 1;
        char expected_error[256] = {0};
        char *allocated_buffer_send = (char*)malloc(cmd_length + 64);

        sprintf(
                expected_error,
                "-ERR the command length has exceeded '%u' bytes\r\n",
                (int)config_module_redis.max_command_length);
        snprintf(
                allocated_buffer_send,
                cmd_length + 64,
                "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$%d\r\n%0*d\r\n",
                cmd_length,
                cmd_length,
                0);
        buffer_send_data_len = strlen(allocated_buffer_send);

        REQUIRE(send(client_fd, allocated_buffer_send, strlen(allocated_buffer_send), 0) == buffer_send_data_len);
        REQUIRE(recv(client_fd, buffer_recv, sizeof(buffer_recv), 0) == strlen(expected_error));
        REQUIRE(strncmp(buffer_recv, expected_error, strlen(expected_error)) == 0);

        free(allocated_buffer_send);
    }

    SECTION("Key too long - Positional") {
        int key_length = (int)config.modules[0].redis->max_key_length + 1;
        char expected_error[256] = { 0 };

        sprintf(
                expected_error,
                "-ERR The %s length has exceeded the allowed size of '%u'\r\n",
                "key",
                (int)config.modules[0].redis->max_key_length);
        snprintf(
                buffer_send,
                sizeof(buffer_send) - 1,
                "*2\r\n$3\r\nGET\r\n$%d\r\n%0*d\r\n",
                key_length,
                key_length,
                0);
        buffer_send_data_len = strlen(buffer_send);

        REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
        REQUIRE(recv(client_fd, buffer_recv, sizeof(buffer_recv), 0) == strlen(expected_error));
        REQUIRE(strncmp(buffer_recv, expected_error, strlen(expected_error)) == 0);
    }

    SECTION("Key too long - Token") {
        int key_length = (int)config.modules[0].redis->max_key_length + 1;
        char expected_error[256] = { 0 };

        sprintf(
                expected_error,
                "-ERR The %s length has exceeded the allowed size of '%u'\r\n",
                "key",
                (int)config.modules[0].redis->max_key_length);
        snprintf(
                buffer_send,
                sizeof(buffer_send) - 1,
                "*4\r\n$4\r\nSORT\r\n$1\r\na\r\n$5\r\nSTORE\r\n$%d\r\n%0*d\r\n",
                key_length,
                key_length,
                0);
        buffer_send_data_len = strlen(buffer_send);

        REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
        REQUIRE(recv(client_fd, buffer_recv, sizeof(buffer_recv), 0) == strlen(expected_error));
        REQUIRE(strncmp(buffer_recv, expected_error, strlen(expected_error)) == 0);
    }

    SECTION("Max command arguments") {
        off_t buffer_send_offset = 0;
        char expected_error[256] = { 0 };
        int arguments_count = (int)config.modules->redis->max_command_arguments + 1;

        buffer_send_offset += snprintf(
                buffer_send + buffer_send_offset,
                sizeof(buffer_send) - buffer_send_offset - 1,
                "*%d\r\n$4\r\nMGET\r\n",
                arguments_count + 1);

        for(int argument_index = 0; argument_index < arguments_count; argument_index++) {
            buffer_send_offset += snprintf(
                    buffer_send + buffer_send_offset,
                    sizeof(buffer_send) - buffer_send_offset - 1,
                    "$11\r\na_key_%05d\r\n",
                    argument_index);
        }

        buffer_send_data_len = strlen(buffer_send);

        sprintf(
                expected_error,
                "-ERR command '%s' has '%u' arguments but only '%u' allowed\r\n",
                "MGET",
                arguments_count,
                (int)config.modules->redis->max_command_arguments);

        REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
        REQUIRE(recv(client_fd, buffer_recv, sizeof(buffer_recv), 0) == strlen(expected_error));
        REQUIRE(strncmp(buffer_recv, expected_error, strlen(expected_error)) == 0);
    }
}