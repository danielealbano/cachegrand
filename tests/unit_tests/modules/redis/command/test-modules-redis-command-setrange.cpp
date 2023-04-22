/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

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
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - SETRANGE", "[redis][command][SETRANGE]") {
    char *expected_response = nullptr;
    char expected_response_static[256] = { 0 };
    size_t long_value_length = 4 * 1024 * 1024;
    config_module_redis.max_command_length = long_value_length + 1024;

    // The long value is, on purpose, not filled with anything to have a very simple fuzzy testing (although
    // it's not repeatable)
    char *long_value = (char *)malloc(long_value_length + 1);

    // Fill with random data the long value
    char range = 'z' - 'a';
    for (size_t i = 0; i < long_value_length; i++) {
        long_value[i] = ((char)i % range) + 'a';
    }

    // This is legit as long_value_length + 1 is actually being allocated
    long_value[long_value_length] = 0;

    SECTION("Invalid offset") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SETRANGE", "a_key", "-10", "b_value"},
                "-ERR offset is out of range\r\n"));
    }

    SECTION("Non-existing key") {
        SECTION("Beginning of first chunk") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", "0", "b_value"},
                    ":7\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    "$7\r\nb_value\r\n"));
        }

        SECTION("Beginning of first chunk - Set 4MB") {
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

            expected_response = (char *)malloc(expected_response_length + 1);
            snprintf(
                    expected_response,
                    expected_response_length + 1,
                    "$%lu\r\n%.*s\r\n",
                    long_value_length,
                    (int) long_value_length,
                    long_value);

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", "0", long_value},
                    (char *) expected_response_static));

            REQUIRE(send_recv_resp_command_multi_recv_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    expected_response,
                    expected_response_length));

            free(expected_response);
        }

        SECTION("Offset 10, padded with nulls") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", "10", "b_value"},
                    ":17\r\n"));

            REQUIRE(send_recv_resp_command_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    "$17\r\n\0\0\0\0\0\0\0\0\0\0b_value\r\n",
                    24));
        }

        SECTION("Pad then overwrite padding") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", "10", "b_value"},
                    ":17\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", "3", "b_value"},
                    ":17\r\n"));

            REQUIRE(send_recv_resp_command_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    "$17\r\n\0\0\0b_valueb_value\r\n",
                    24));
        }

        SECTION("Pad then pad again padding") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", "10", "b_value"},
                    ":17\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", "20", "value_z"},
                    ":27\r\n"));

            REQUIRE(send_recv_resp_command_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    "$27\r\n\0\0\0\0\0\0\0\0\0\0b_value\0\0\0value_z\r\n",
                    34));
        }

        SECTION("Pad till a few bytes before the end of first chunk and then write at the beginning of the second") {
            char offset_str[16] = { 0 };
            off_t offset = STORAGE_DB_CHUNK_MAX_SIZE - 3;
            snprintf(offset_str, sizeof(offset_str), "%ld", offset);

            char expected[STORAGE_DB_CHUNK_MAX_SIZE + 64] = { 0 };
            size_t header_len = snprintf(expected, sizeof(expected), "$%lu\r\n", offset + strlen("b_value"));
            memcpy(expected + header_len + offset, "b_value\r\n", strlen("b_value\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", offset_str, "b_value"},
                    ":65539\r\n"));

            REQUIRE(send_recv_resp_command_multi_recv_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    (char *) expected,
                    header_len + offset + strlen("b_value\r\n")));
        }
    }

    SECTION("Existing key") {
        char *value1 = "value_f";
        size_t value1_len = strlen(value1);
        char *value2 = "b_value";
        size_t value2_len = strlen(value2);

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SET", "a_key", value1},
                "+OK\r\n"));

        SECTION("Beginning of first chunk") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", "0", value2},
                    ":7\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    "$7\r\nb_value\r\n"));
        }

        SECTION("Beginning of first chunk - Set 4MB") {
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

            expected_response = (char *)malloc(expected_response_length + 1);
            snprintf(
                    expected_response,
                    expected_response_length + 1,
                    "$%lu\r\n%.*s\r\n",
                    long_value_length,
                    (int) long_value_length,
                    long_value);

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", "0", long_value},
                    (char *) expected_response_static));

            REQUIRE(send_recv_resp_command_multi_recv_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    expected_response,
                    expected_response_length));

            free(expected_response);
        }

        SECTION("Beginning of first chunk and after 4MB - Set 4MB") {
            snprintf(
                    (char*)expected_response_static,
                    sizeof(expected_response_static),
                    ":%lu\r\n",
                    long_value_length);
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", "0", long_value},
                    (char *) expected_response_static));

            char offset[32] = { 0 };
            snprintf(offset, sizeof(offset), "%lu", long_value_length);

            snprintf(
                    (char*)expected_response_static,
                    sizeof(expected_response_static),
                    ":%lu\r\n",
                    long_value_length * 2);
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", offset, long_value},
                    (char *) expected_response_static));

            size_t expected_response_length = snprintf(
                    nullptr,
                    0,
                    "$%lu\r\n%.*s%.*s\r\n",
                    long_value_length * 2,
                    (int) long_value_length,
                    long_value,
                    (int) long_value_length,
                    long_value);

            expected_response = (char *)malloc(expected_response_length + 1);
            snprintf(
                    expected_response,
                    expected_response_length + 1,
                    "$%lu\r\n%.*s%.*s\r\n",
                    long_value_length * 2,
                    (int) long_value_length,
                    long_value,
                    (int) long_value_length,
                    long_value);
            REQUIRE(send_recv_resp_command_multi_recv_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    expected_response,
                    expected_response_length));

            free(expected_response);
        }

        SECTION("Offset 10, padded with nulls") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", "10", value2},
                    ":17\r\n"));

            REQUIRE(send_recv_resp_command_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    "$17\r\nvalue_f\0\0\0b_value\r\n",
                    24));
        }

        SECTION("Pad then overwrite padding") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", "10", value2},
                    ":17\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", "3", value2},
                    ":17\r\n"));

            REQUIRE(send_recv_resp_command_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    "$17\r\nvalb_valueb_value\r\n",
                    24));
        }

        SECTION("Pad then pad again padding") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", "10", value2},
                    ":17\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", "20", value2},
                    ":27\r\n"));

            REQUIRE(send_recv_resp_command_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    "$27\r\nvalue_f\0\0\0b_value\0\0\0b_value\r\n",
                    34));
        }

        SECTION("Pad till a few bytes before the end of first chunk and then write at the beginning of the second") {
            char offset_str[16] = { 0 };
            off_t offset = STORAGE_DB_CHUNK_MAX_SIZE - 3;
            snprintf(offset_str, sizeof(offset_str), "%ld", offset);

            char expected[STORAGE_DB_CHUNK_MAX_SIZE + 64] = { 0 };
            size_t header_len = snprintf(
                    expected,
                    sizeof(expected),
                    "$%lu\r\n%s", offset + value2_len, value1);
            memcpy(expected + header_len + offset - value1_len, value2, value2_len);
            memcpy(expected + header_len + offset - value1_len + value2_len, "\r\n", strlen("\r\n"));
            size_t expected_len = header_len + offset - value1_len + value2_len + 2;

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SETRANGE", "a_key", offset_str, value2},
                    ":65539\r\n"));

            REQUIRE(send_recv_resp_command_multi_recv_and_validate_recv(
                    std::vector<std::string>{"GET", "a_key"},
                    (char *) expected,
                    expected_len));
        }
    }

    free(long_value);
}
