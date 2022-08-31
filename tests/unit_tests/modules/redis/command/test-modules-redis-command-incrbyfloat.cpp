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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - INCRBYFLOAT", "[redis][command][INCRBYFLOAT]") {
    SECTION("Non-existing key") {
        SECTION("Increase 1 - once") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"INCRBYFLOAT", "a_key", "1"},
                    "$1\r\n1\r\n"));
        }

        SECTION("Increase 1 - twice") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"INCRBYFLOAT", "a_key", "1"},
                    "$1\r\n1\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"INCRBYFLOAT", "a_key", "1"},
                    "$1\r\n2\r\n"));
        }

        SECTION("Increase 1.5 - once") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"INCRBYFLOAT", "a_key", "1.5"},
                    "$3\r\n1.5\r\n"));
        }

        SECTION("Increase 1.5 - twice") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"INCRBYFLOAT", "a_key", "1.5"},
                    "$3\r\n1.5\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"INCRBYFLOAT", "a_key", "1.5"},
                    "$1\r\n3\r\n"));
        }

        SECTION("Increase integer amount - once") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"INCRBYFLOAT", "a_key", "5"},
                    "$1\r\n5\r\n"));
        }

        SECTION("Increase integer amount - twice") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"INCRBYFLOAT", "a_key", "5"},
                    "$1\r\n5\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"INCRBYFLOAT", "a_key", "6"},
                    "$2\r\n11\r\n"));
        }

        SECTION("Increase decimal amount - once") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"INCRBYFLOAT", "a_key", "5.5"},
                    "$3\r\n5.5\r\n"));
        }

        SECTION("Increase decimal amount - twice") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"INCRBYFLOAT", "a_key", "5.5"},
                    "$3\r\n5.5\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"INCRBYFLOAT", "a_key", "6.4"},
                    "$4\r\n11.9\r\n"));
        }
    }

    SECTION("Existing key") {
        SECTION("Simple positive number") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "5"},
                    "+OK\r\n"));

            SECTION("Increase 1 - once") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1"},
                        "$1\r\n6\r\n"));
            }

            SECTION("Increase 1 - twice") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1"},
                        "$1\r\n6\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1"},
                        "$1\r\n7\r\n"));
            }

            SECTION("Increase 1.5 - once") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1.5"},
                        "$3\r\n6.5\r\n"));
            }

            SECTION("Increase 1.5 - twice") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1.5"},
                        "$3\r\n6.5\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1.5"},
                        "$1\r\n8\r\n"));
            }

            SECTION("Increase integer amount - once") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "5"},
                        "$2\r\n10\r\n"));
            }

            SECTION("Increase integer amount - twice") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "5"},
                        "$2\r\n10\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "6"},
                        "$2\r\n16\r\n"));
            }

            SECTION("Increase decimal amount - once") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "5.5"},
                        "$4\r\n10.5\r\n"));
            }

            SECTION("Increase decimal amount - twice") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "5.5"},
                        "$4\r\n10.5\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "6.4"},
                        "$4\r\n16.9\r\n"));
            }
        }

        SECTION("Simple negative number") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "-5"},
                    "+OK\r\n"));

            SECTION("Increase 1 - once") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1"},
                        "$2\r\n-4\r\n"));
            }

            SECTION("Increase 1 - twice") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1"},
                        "$2\r\n-4\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1"},
                        "$2\r\n-3\r\n"));
            }

            SECTION("Increase 1.5 - once") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1.5"},
                        "$4\r\n-3.5\r\n"));
            }

            SECTION("Increase 1.5 - twice") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1.5"},
                        "$4\r\n-3.5\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1.5"},
                        "$2\r\n-2\r\n"));
            }

            SECTION("Increase integer amount - once") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "5"},
                        "$1\r\n0\r\n"));
            }

            SECTION("Increase integer amount - twice") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "5"},
                        "$1\r\n0\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "6"},
                        "$1\r\n6\r\n"));
            }

            SECTION("Increase decimal amount - once") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "5.5"},
                        "$3\r\n0.5\r\n"));
            }

            SECTION("Increase decimal amount - twice") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "5.5"},
                        "$3\r\n0.5\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "6.4"},
                        "$3\r\n6.9\r\n"));
            }
        }

        SECTION("Increment INT64_MIN") {
            SECTION("Integer") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "-9223372036854775808"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1"},
                        "$20\r\n-9223372036854775807\r\n"));
            }

            SECTION("Decimal") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "-9223372036854775808"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1.6"},
                        "$20\r\n-9223372036854775806\r\n"));
            }
        }

        SECTION("Non numeric") {
            SECTION("String") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "b_value"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1"},
                        "-ERR value is not an integer or out of range\r\n"));
            }

            SECTION("Greater than INT64_MAX") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "9223372036854775808"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1"},
                        "-ERR value is not an integer or out of range\r\n"));
            }

            SECTION("Smaller than INT64_MIN") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "-9223372036854775809"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1"},
                        "-ERR value is not an integer or out of range\r\n"));
            }
        }

        SECTION("Overflow number") {
            SECTION("One") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "9223372036854775806"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1"},
                        "$19\r\n9223372036854775807\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1"},
                        "-ERR increment or decrement would overflow\r\n"));
            }

            SECTION("Integer amount") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "9223372036854775806"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "2"},
                        "-ERR increment or decrement would overflow\r\n"));
            }

            SECTION("Decimal amount") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "9223372036854775806"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"INCRBYFLOAT", "a_key", "1.5"},
                        "-ERR increment or decrement would overflow\r\n"));
            }
        }
    }
}
