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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - MSETNX", "[redis][command][MSETNX]") {
    SECTION("1 key") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MSETNX", "a_key", "b_value"},
                ":1\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET", "a_key"},
                "$7\r\nb_value\r\n"));
    }

    SECTION("2 keys") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MSETNX", "a_key", "b_value", "b_key", "value_z"},
                ":1\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET", "a_key"},
                "$7\r\nb_value\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET", "b_key"},
                "$7\r\nvalue_z\r\n"));
    }

    SECTION("Existing keys") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MSETNX", "a_key", "b_value", "b_key", "value_z"},
                ":1\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET", "a_key"},
                "$7\r\nb_value\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET", "b_key"},
                "$7\r\nvalue_z\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MSETNX", "b_key", "another_value", "c_key", "value_not_being_set"},
                ":0\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET", "b_key"},
                "$7\r\nvalue_z\r\n"));

        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"GET", "c_key"},
                "$-1\r\n"));
    }

        // TODO: need to fix rmw multi key transactions first
//        SECTION("Set 64 keys") {
//            int key_count = 64;
//            std::vector<std::string> arguments = std::vector<std::string>();
//
//            arguments.emplace_back("MSETNX");
//            for(int key_index = 0; key_index < key_count; key_index++) {
//                arguments.push_back(string_format("a_key_%05d", key_index));
//                arguments.push_back(string_format("b_value_%05d", key_index));
//            }
//
//            REQUIRE(send_recv_resp_command_text(
//                    client_fd,
//                    arguments,
//                    ":1\r\n"));
//
//            for(int key_index = 0; key_index < key_count; key_index++) {
//                char expected_response[32] = { 0 };
//                REQUIRE(send_recv_resp_command_text(
//                        client_fd,
//                        std::vector<std::string>{ "GET", string_format("a_key_%05d", key_index) },
//                        (char*)(string_format("$13\r\nb_value_%05d\r\n", key_index).c_str())));
//            }
//        }

    SECTION("Missing parameters - key") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MSETNX"},
                "-ERR wrong number of arguments for 'MSETNX' command\r\n"));
    }

    SECTION("Missing parameters - value") {
        REQUIRE(send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"MSETNX", "a_key"},
                "-ERR wrong number of arguments for 'MSETNX' command\r\n"));
    }
}