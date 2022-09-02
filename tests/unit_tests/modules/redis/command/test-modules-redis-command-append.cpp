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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - APPEND", "[redis][command][APPEND]") {
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

    SECTION("Non-existing key") {
        SECTION("Append once") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"APPEND", "a_key", "b_value"},
                    ":7\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    "$7\r\nb_value\r\n"));
        }

        SECTION("Append twice") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"APPEND", "a_key", "b_value"},
                    ":7\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"APPEND", "a_key", "c_value"},
                    ":14\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    "$14\r\nb_valuec_value\r\n"));
        }

        SECTION("Append at least one chunk") {
            char value[STORAGE_DB_CHUNK_MAX_SIZE + 64] = { 0 };
            char expected[STORAGE_DB_CHUNK_MAX_SIZE + 128] = { 0 };

            assert(sizeof(value) <= long_value_length);
            memcpy(value, long_value, sizeof(value));

            snprintf(
                    expected,
                    sizeof(expected),
                    ":%lu\r\n",
                    sizeof(value));
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"APPEND", "a_key", value},
                    expected));

            size_t expected_length = snprintf(
                    expected,
                    sizeof(expected),
                    "$%lu\r\n%.*s\r\n",
                    sizeof(value),
                    (int)sizeof(value),
                    value);
            REQUIRE(send_recv_resp_command_multi_recv_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    (char *) expected,
                    expected_length,
                    send_recv_resp_command_calculate_multi_recv(expected_length)));
        }

        SECTION("Append once - 4MB") {
            char expected_response_static[256] = { 0 };
            snprintf(
                    (char*)expected_response_static,
                    sizeof(expected_response_static),
                    ":%lu\r\n",
                    long_value_length);

            size_t expected_response_length = snprintf(
                    nullptr,
                    0,
                    "$%lu\r\n%.*s\r\n",
                    long_value_length,
                    (int) long_value_length,
                    long_value);

            char *expected_response = (char *)malloc(expected_response_length + 1);
            snprintf(
                    expected_response,
                    expected_response_length + 1,
                    "$%lu\r\n%.*s\r\n",
                    long_value_length,
                    (int) long_value_length,
                    long_value);

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"APPEND", "a_key", long_value},
                    (char *) expected_response_static));

            REQUIRE(send_recv_resp_command_multi_recv_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    expected_response,
                    expected_response_length,
                    send_recv_resp_command_calculate_multi_recv(long_value_length)));

            free(expected_response);
        }

        SECTION("Append twice - 4MB") {
            char expected_response_static[256] = { 0 };
            snprintf(
                    (char*)expected_response_static,
                    sizeof(expected_response_static),
                    ":%lu\r\n",
                    long_value_length);

            size_t expected_response_length = snprintf(
                    nullptr,
                    0,
                    "$%lu\r\n%.*s\r\n",
                    long_value_length,
                    (int) long_value_length,
                    long_value);

            char *expected_response = (char *)malloc(expected_response_length + 1);
            snprintf(
                    expected_response,
                    expected_response_length + 1,
                    "$%lu\r\n%.*s\r\n",
                    long_value_length,
                    (int) long_value_length,
                    long_value);

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"APPEND", "a_key", long_value},
                    (char *) expected_response_static));

            REQUIRE(send_recv_resp_command_multi_recv_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    expected_response,
                    expected_response_length,
                    send_recv_resp_command_calculate_multi_recv(expected_response_length)));

            free(expected_response);
        }
    }

    SECTION("Existing key") {
        char *value1 = "value_f";
        size_t value1_len = strlen(value1);
        char *value2 = "b_value";

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SET", "a_key", value1},
                "+OK\r\n"));

        SECTION("Append once") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"APPEND", "a_key", value2},
                    ":14\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    "$14\r\nvalue_fb_value\r\n"));
        }

        SECTION("Append twice") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"APPEND", "a_key", value2},
                    ":14\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"APPEND", "a_key", value1},
                    ":21\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    "$21\r\nvalue_fb_valuevalue_f\r\n"));
        }

        SECTION("Append at least one chunk") {
            char value[STORAGE_DB_CHUNK_MAX_SIZE + 64] = { 0 };
            char expected[STORAGE_DB_CHUNK_MAX_SIZE + 128] = { 0 };

            assert(sizeof(value) <= long_value_length);
            memcpy(value, long_value, sizeof(value));

            snprintf(
                    expected,
                    sizeof(expected),
                    ":%lu\r\n",
                    value1_len + sizeof(value));
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"APPEND", "a_key", value},
                    expected));

            size_t expected_length = snprintf(
                    expected,
                    sizeof(expected),
                    "$%lu\r\n%s%.*s\r\n",
                    value1_len + sizeof(value),
                    value1,
                    (int)sizeof(value),
                    value);
            REQUIRE(send_recv_resp_command_multi_recv_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    (char *) expected,
                    expected_length,
                    send_recv_resp_command_calculate_multi_recv(expected_length)));
        }

        SECTION("Append once - 4MB") {
            char expected_response_static[256] = { 0 };
            snprintf(
                    (char*)expected_response_static,
                    sizeof(expected_response_static),
                    ":%lu\r\n",
                    value1_len + long_value_length);

            size_t expected_response_length = snprintf(
                    nullptr,
                    0,
                    "$%lu\r\n%s%.*s\r\n",
                    value1_len + long_value_length,
                    value1,
                    (int) long_value_length,
                    long_value);

            char *expected_response = (char *)malloc(expected_response_length + 1);
            snprintf(
                    expected_response,
                    expected_response_length + 1,
                    "$%lu\r\n%s%.*s\r\n",
                    value1_len + long_value_length,
                    value1,
                    (int) long_value_length,
                    long_value);

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"APPEND", "a_key", long_value},
                    (char *) expected_response_static));

            REQUIRE(send_recv_resp_command_multi_recv_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    expected_response,
                    expected_response_length,
                    send_recv_resp_command_calculate_multi_recv(expected_response_length)));

            free(expected_response);
        }

        SECTION("Append twice - 4MB") {
            char expected_response_static1[64] = { 0 };
            char expected_response_static2[64] = { 0 };
            snprintf(
                    (char*)expected_response_static1,
                    sizeof(expected_response_static1),
                    ":%lu\r\n",
                    value1_len + long_value_length);
            snprintf(
                    (char*)expected_response_static2,
                    sizeof(expected_response_static2),
                    ":%lu\r\n",
                    value1_len + (long_value_length) * 2);

            size_t expected_response_length = snprintf(
                    nullptr,
                    0,
                    "$%lu\r\n%s%.*s%.*s\r\n",
                    value1_len + (long_value_length * 2),
                    value1,
                    (int) long_value_length,
                    long_value,
                    (int) long_value_length,
                    long_value);

            char *expected_response = (char *)malloc(expected_response_length + 1);
            snprintf(
                    expected_response,
                    expected_response_length + 1,
                    "$%lu\r\n%s%.*s%.*s\r\n",
                    value1_len + (long_value_length * 2),
                    value1,
                    (int) long_value_length,
                    long_value,
                    (int) long_value_length,
                    long_value);

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"APPEND", "a_key", long_value},
                    (char *) expected_response_static1));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"APPEND", "a_key", long_value},
                    (char *) expected_response_static2));

            REQUIRE(send_recv_resp_command_multi_recv_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    expected_response,
                    expected_response_length,
                    send_recv_resp_command_calculate_multi_recv(expected_response_length)));

            free(expected_response);
        }
    }

    free(long_value);
}
