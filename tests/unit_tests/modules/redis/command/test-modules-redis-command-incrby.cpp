/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - INCRBY", "[redis][command][INCRBY]") {
    SECTION("Non-existing key") {
        SECTION("Increase 1 - once") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"INCRBY", "a_key", "1"},
                    ":1\r\n"));
        }

        SECTION("Increase 1 - twice") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"INCRBY", "a_key", "1"},
                    ":1\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"INCRBY", "a_key", "1"},
                    ":2\r\n"));
        }

        SECTION("Increase amount - once") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"INCRBY", "a_key", "5"},
                    ":5\r\n"));
        }

        SECTION("Increase amount - twice") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"INCRBY", "a_key", "5"},
                    ":5\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"INCRBY", "a_key", "6"},
                    ":11\r\n"));
        }
    }

    SECTION("Existing key") {
        SECTION("Simple positive number") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SET", "a_key", "5"},
                    "+OK\r\n"));

            SECTION("Increase 1 - once") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "1"},
                        ":6\r\n"));
            }

            SECTION("Increase 1 - twice") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "1"},
                        ":6\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "1"},
                        ":7\r\n"));
            }

            SECTION("Increase amount - once") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "5"},
                        ":10\r\n"));
            }

            SECTION("Increase amount - twice") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "5"},
                        ":10\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "6"},
                        ":16\r\n"));
            }
        }

        SECTION("Simple negative number") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SET", "a_key", "-5"},
                    "+OK\r\n"));

            SECTION("Increase 1 - once") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "1"},
                        ":-4\r\n"));
            }

            SECTION("Increase 1 - twice") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "1"},
                        ":-4\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "1"},
                        ":-3\r\n"));
            }

            SECTION("Increase amount - once") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "5"},
                        ":0\r\n"));
            }

            SECTION("Increase amount - twice") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "5"},
                        ":0\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "6"},
                        ":6\r\n"));
            }
        }

        SECTION("Increment INT64_MIN") {
            SECTION("One") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SET", "a_key", "-9223372036854775808"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "1"},
                        ":-9223372036854775807\r\n"));
            }
        }

        SECTION("Non numeric") {
            SECTION("String") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SET", "a_key", "b_value"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "1"},
                        "-ERR value is not an integer or out of range\r\n"));
            }

            SECTION("Greater than INT64_MAX") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SET", "a_key", "9223372036854775808"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "1"},
                        "-ERR value is not an integer or out of range\r\n"));
            }

            SECTION("Smaller than INT64_MIN") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SET", "a_key", "-9223372036854775809"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "1"},
                        "-ERR value is not an integer or out of range\r\n"));
            }
        }

        SECTION("Overflow number") {
            SECTION("One") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SET", "a_key", "9223372036854775806"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "1"},
                        ":9223372036854775807\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "1"},
                        "-ERR increment or decrement would overflow\r\n"));
            }

            SECTION("Amount") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SET", "a_key", "9223372036854775806"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"INCRBY", "a_key", "2"},
                        "-ERR increment or decrement would overflow\r\n"));
            }
        }
    }
}
