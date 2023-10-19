/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include <cstdbool>
#include <memory>

#include <netinet/in.h>

#include "clock.h"
#include "exttypes.h"
#include "spinlock.h"
#include "transaction.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "config.h"
#include "fiber/fiber.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "signal_handler_thread.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"

#include "program.h"

#include "test-modules-redis-command-fixture.hpp"

#pragma GCC diagnostic ignored "-Wwrite-strings"

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - SELECT", "[redis][command][SELECT]") {
    SECTION("Select DB 1") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SELECT", "1"},
                "+OK\r\n"));
    }

    SECTION("Select DB 15 - allowed") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SELECT", "15"},
                "+OK\r\n"));
    }

    SECTION("Select DB 16 - not allowed") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SELECT", "16"},
                "-ERR invalid DB index\r\n"));
    }

    SECTION("SET, SELECT and GET") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SET", "a_key", "b_value"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"GET", "a_key"},
                "$7\r\nb_value\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SELECT", "1"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"GET", "a_key"},
                "$-1\r\n"));
    }

    SECTION("SET, SELECT, SET same key diff value, switch back and GET") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SET", "a_key", "b_value"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"GET", "a_key"},
                "$7\r\nb_value\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SELECT", "1"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SET", "a_key", "z_value"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SELECT", "0"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"GET", "a_key"},
                "$7\r\nb_value\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SELECT", "1"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"GET", "a_key"},
                "$7\r\nz_value\r\n"));
    }
}
