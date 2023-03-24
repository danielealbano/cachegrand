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
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - PTTL", "[redis][command][PTTL]") {
    SECTION("Non-existing key") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"PTTL", "a_key"},
                ":-2\r\n"));
    }

    SECTION("Key without expiry") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SET", "a_key", "b_value"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"PTTL", "a_key"},
                ":-1\r\n"));
    }

    SECTION("Key with 5 second expire") {
        char buffer[32] = { 0 };
        size_t out_buffer_length = 0;
        int64_t unixtime_response;
        size_t expected_length = snprintf(nullptr, 0, ":%d\r\n", 5000);

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SET", "a_key", "b_value", "EX", "5"},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_multi_recv(
                std::vector<std::string>{"PTTL", "a_key"},
                buffer,
                &out_buffer_length,
                expected_length));

        unixtime_response = strtoll(buffer + 1, nullptr, 10);

        REQUIRE((unixtime_response >= 5000 - 5 && unixtime_response <= 5000));
    }
}
