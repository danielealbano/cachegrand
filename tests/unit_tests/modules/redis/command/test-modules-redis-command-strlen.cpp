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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - STRLEN", "[redis][command][STRLEN]") {
    SECTION("Existing key") {
        SECTION("Short key") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"STRLEN", "a_key"},
                    ":7\r\n"));
        }

        SECTION("Long key") {
            char expected[64] = { 0 };
            size_t long_value_length = 4 * 1024 * 1024;
            config_module_redis.max_command_length = long_value_length + 1024;

            // The long value is, on purpose, not filled with anything to have a very simple fuzzy testing (although
            // it's not repeatable)
            char *long_value = (char *) malloc(long_value_length + 1);

            // Fill with random data the long value
            char range = 'z' - 'a';
            for (size_t i = 0; i < long_value_length; i++) {
                long_value[i] = (char)(i % range) + 'a';
            }

            // This is legit as long_value_length + 1 is actually being allocated
            long_value[long_value_length] = 0;

            snprintf(
                    expected,
                    sizeof(expected),
                    ":%lu\r\n",
                    long_value_length);

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SET", "a_key", long_value},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"STRLEN", "a_key"},
                    expected));
        }
    }

    SECTION("Not-existing key") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"STRLEN", "a_key"},
                ":0\r\n"));
    }
}
