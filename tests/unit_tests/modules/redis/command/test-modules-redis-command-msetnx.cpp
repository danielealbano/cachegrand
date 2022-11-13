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
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "config.h"
#include "fiber/fiber.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "signal_handler_thread.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "epoch_gc.h"
#include "epoch_gc_worker.h"

#include "program.h"

#include "test-modules-redis-command-fixture.hpp"

#pragma GCC diagnostic ignored "-Wwrite-strings"

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - MSETNX", "[redis][command][MSETNX]") {
    SECTION("1 key") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"MSETNX", "a_key", "b_value"},
                ":1\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"GET", "a_key"},
                "$7\r\nb_value\r\n"));
    }

    SECTION("2 keys") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"MSETNX", "a_key", "b_value", "b_key", "value_z"},
                ":1\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"GET", "a_key"},
                "$7\r\nb_value\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"GET", "b_key"},
                "$7\r\nvalue_z\r\n"));
    }

    SECTION("Existing keys") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"MSETNX", "a_key", "b_value", "b_key", "value_z"},
                ":1\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"GET", "a_key"},
                "$7\r\nb_value\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"GET", "b_key"},
                "$7\r\nvalue_z\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"MSETNX", "b_key", "another_value", "c_key", "value_not_being_set"},
                ":0\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"GET", "b_key"},
                "$7\r\nvalue_z\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"GET", "c_key"},
                "$-1\r\n"));
    }

    SECTION("Set 64 keys") {
        int key_count = 64;
        std::vector<std::string> arguments = std::vector<std::string>();

        arguments.emplace_back("MSETNX");
        for(int key_index = 0; key_index < key_count; key_index++) {
            char buffer1[32] = { 0 };
            char buffer2[32] = { 0 };
            snprintf(buffer1, sizeof(buffer1), "a_key_%05d", key_index);
            snprintf(buffer2, sizeof(buffer2), "b_value_%05d", key_index);

            arguments.push_back(buffer1);
            arguments.push_back(buffer2);
        }

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                arguments,
                ":1\r\n"));

        for(int key_index = 0; key_index < key_count; key_index++) {
            char buffer1[32] = { 0 };
            char expected_response[64] = { 0 };
            snprintf(buffer1, sizeof(buffer1), "a_key_%05d", key_index);
            snprintf(expected_response, sizeof(expected_response), "$13\r\nb_value_%05d\r\n", key_index);

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"GET", buffer1},
                    expected_response));
        }
    }

    SECTION("Missing parameters - key") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"MSETNX"},
                "-ERR wrong number of arguments for 'msetnx' command\r\n"));
    }

    SECTION("Missing parameters - value") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"MSETNX", "a_key"},
                "-ERR wrong number of arguments for 'msetnx' command\r\n"));
    }
}
