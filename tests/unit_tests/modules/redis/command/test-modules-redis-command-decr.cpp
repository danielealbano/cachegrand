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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - DECR", "[redis][command][DECR]") {
    SECTION("Non-existing key") {
        SECTION("Decrease once") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"DECR", "a_key"},
                    ":-1\r\n"));
        }

        SECTION("Decrease twice") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"DECR", "a_key"},
                    ":-1\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"DECR", "a_key"},
                    ":-2\r\n"));
        }
    }

    SECTION("Existing key") {
        SECTION("Simple negative number") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SET", "a_key", "-5"},
                    "+OK\r\n"));

            SECTION("Decrease once") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"DECR", "a_key"},
                        ":-6\r\n"));
            }

            SECTION("Decrease twice") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"DECR", "a_key"},
                        ":-6\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"DECR", "a_key"},
                        ":-7\r\n"));
            }
        }

        SECTION("Simple positive number") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SET", "a_key", "5"},
                    "+OK\r\n"));

            SECTION("Decrease once") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"DECR", "a_key"},
                        ":4\r\n"));
            }

            SECTION("Decrease twice") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"DECR", "a_key"},
                        ":4\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"DECR", "a_key"},
                        ":3\r\n"));
            }
        }

        SECTION("Decrease INT64_MAX") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SET", "a_key", "9223372036854775807"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"DECR", "a_key"},
                    ":9223372036854775806\r\n"));
        }

        SECTION("Non numeric") {
            SECTION("String") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SET", "a_key", "b_value"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"DECR", "a_key"},
                        "-ERR value is not an integer or out of range\r\n"));
            }

            SECTION("Greater than INT64_MAX") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SET", "a_key", "9223372036854775808"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"DECR", "a_key"},
                        "-ERR value is not an integer or out of range\r\n"));
            }

            SECTION("Smaller than INT64_MIN") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SET", "a_key", "-9223372036854775809"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"DECR", "a_key"},
                        "-ERR value is not an integer or out of range\r\n"));
            }
        }

        SECTION("Overflow number") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SET", "a_key", "-9223372036854775807"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"DECR", "a_key"},
                    ":-9223372036854775808\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"DECR", "a_key"},
                    "-ERR increment or decrement would overflow\r\n"));
        }
    }
}
