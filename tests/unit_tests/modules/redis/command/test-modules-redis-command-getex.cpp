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

#include <unistd.h>
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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - GETEX", "[redis][command][GETEX]") {
    SECTION("Existing key") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GETEX", "a_key"},
                "$7\r\nb_value\r\n"));
    }

    SECTION("Non-existing key") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GETEX", "a_key"},
                "$-1\r\n"));
    }

    SECTION("New key - expire in 500ms") {
        config_module_network_timeout.read_ms = 1000;

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GETEX", "a_key", "PX", "500"},
                "$7\r\nb_value\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET", "a_key"},
                "$7\r\nb_value\r\n"));

        // Wait for 600 ms and try to get the value after the expiration
        usleep((500 + 100) * 1000);

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET", "a_key"},
                "$-1\r\n"));

        storage_db_entry_index_t *entry_index = storage_db_get_entry_index(db, "a_key", strlen("a_key"));
        REQUIRE(entry_index == NULL);
    }

    SECTION("New key - expire in 1s") {
        config_module_network_timeout.read_ms = 2000;

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SET", "a_key", "b_value"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GETEX", "a_key", "EX", "1"},
                "$7\r\nb_value\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET", "a_key"},
                "$7\r\nb_value\r\n"));

        // Wait for 1100 ms and try to get the value after the expiration
        usleep((1000 + 100) * 1000);

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET", "a_key"},
                "$-1\r\n"));

        storage_db_entry_index_t *entry_index = storage_db_get_entry_index(db, "a_key", strlen("a_key"));
        REQUIRE(entry_index == NULL);
    }
}