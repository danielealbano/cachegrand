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
#include "data_structures/ring_bounded_spsc/ring_bounded_spsc.h"
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

#include "program.h"

#include "test-modules-redis-command-fixture.hpp"

#pragma GCC diagnostic ignored "-Wwrite-strings"

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - SCAN", "[redis][command][SCAN]") {
    SECTION("Unsupported TYPE token") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SCAN", "0", "TYPE"},
                "-ERR the TYPE parameter is not yet supported\r\n"));
    }

    SECTION("Empty database") {
        SECTION("No pattern and no count") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SCAN", "0"},
                    "*2\r\n:0\r\n*0\r\n"));
        }

        SECTION("No matching pattern and no count") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SCAN", "0", "MATCH", "nomatch"},
                    "*2\r\n:0\r\n*0\r\n"));
        }

        SECTION("No matching pattern and with count") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"SCAN", "0", "MATCH", "nomatch", "COUNT", "10"},
                    "*2\r\n:0\r\n*0\r\n"));
        }
    }

    SECTION("One key") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"MSET", "a_key", "b_value"},
                "+OK\r\n"));

        SECTION("With count") {
            SECTION("No match") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "nomatch", "COUNT", "100"},
                        "*2\r\n:687\r\n*0\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "687", "MATCH", "nomatch", "COUNT", "100"},
                        "*2\r\n:0\r\n*0\r\n"));
            }

            SECTION("Match - simple") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "a_key", "COUNT", "100"},
                        "*2\r\n:687\r\n*1\r\n$5\r\na_key\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "687", "MATCH", "a_key", "COUNT", "100"},
                        "*2\r\n:0\r\n*0\r\n"));
            }

            SECTION("Only count") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "COUNT", "100"},
                        "*2\r\n:687\r\n*1\r\n$5\r\na_key\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "687", "COUNT", "100"},
                        "*2\r\n:0\r\n*0\r\n"));
            }
        }

        SECTION("With pattern") {
            SECTION("No match") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "nomatch"},
                        "*2\r\n:0\r\n*0\r\n"));
            }

            SECTION("Match - simple") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "a_key"},
                        "*2\r\n:0\r\n*1\r\n$5\r\na_key\r\n"));
            }

            SECTION("Match - star") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "a_*"},
                        "*2\r\n:0\r\n*1\r\n$5\r\na_key\r\n"));
            }

            SECTION("Match - question mark") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "a_???"},
                        "*2\r\n:0\r\n*1\r\n$5\r\na_key\r\n"));
            }

            SECTION("Match - backslash") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "a\\_key"},
                        "*2\r\n:0\r\n*1\r\n$5\r\na_key\r\n"));
            }

            SECTION("Match - brackets") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "[a-z]_key"},
                        "*2\r\n:0\r\n*1\r\n$5\r\na_key\r\n"));
            }

            SECTION("Match - everything") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "*"},
                        "*2\r\n:0\r\n*1\r\n$5\r\na_key\r\n"));
            }
        }
    }

    SECTION("Multiple keys") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{
                        "MSET",
                        "a_key", "a_value",
                        "b_key", "b_value",
                        "c_key", "c_value",
                        "d_key", "d_value",
                        "key_zzz", "value_z"},
                "+OK\r\n"));

        SECTION("With count") {
            SECTION("No match") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "nomatch", "COUNT", "100"},
                        "*2\r\n:281\r\n*0\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "281", "MATCH", "nomatch", "COUNT", "100"},
                        "*2\r\n:687\r\n*0\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "687", "MATCH", "nomatch", "COUNT", "100"},
                        "*2\r\n:841\r\n*0\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "841", "MATCH", "nomatch", "COUNT", "100"},
                        "*2\r\n:0\r\n*0\r\n"));
            }

            SECTION("Match - simple") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "a_key", "COUNT", "100"},
                        "*2\r\n:281\r\n*0\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "281", "MATCH", "a_key", "COUNT", "100"},
                        "*2\r\n:687\r\n*1\r\n$5\r\na_key\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "687", "MATCH", "a_key", "COUNT", "100"},
                        "*2\r\n:841\r\n*0\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "841", "MATCH", "nomatch", "COUNT", "100"},
                        "*2\r\n:0\r\n*0\r\n"));
            }

            SECTION("Only count") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "COUNT", "100"},
                        "*2\r\n:281\r\n*2\r\n$7\r\nkey_zzz\r\n$5\r\nb_key\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "281", "COUNT", "100"},
                        "*2\r\n:687\r\n*1\r\n$5\r\na_key\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "687", "COUNT", "100"},
                        "*2\r\n:841\r\n*2\r\n$5\r\nd_key\r\n$5\r\nc_key\r\n"));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "841", "COUNT", "100"},
                        "*2\r\n:0\r\n*0\r\n"));
            }
        }

        SECTION("With pattern") {
            SECTION("No match") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "nomatch"},
                        "*2\r\n:0\r\n*0\r\n"));
            }

            SECTION("Match - simple") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "a_key"},
                        "*2\r\n:0\r\n*1\r\n$5\r\na_key\r\n"));
            }

            SECTION("Match - star - 1 result") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "a_*"},
                        "*2\r\n:0\r\n*1\r\n$5\r\na_key\r\n"));
            }

            SECTION("Match - star - multiple results") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "*key"},
                        "*2\r\n:0\r\n*4\r\n$5\r\nb_key\r\n$5\r\na_key\r\n$5\r\nd_key\r\n$5\r\nc_key\r\n"));
            }

            SECTION("Match - question mark") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "a_???"},
                        "*2\r\n:0\r\n*1\r\n$5\r\na_key\r\n"));
            }

            SECTION("Match - backslash") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "a\\_key"},
                        "*2\r\n:0\r\n*1\r\n$5\r\na_key\r\n"));
            }

            SECTION("Match - brackets") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "[a-z]_key"},
                        "*2\r\n:0\r\n*4\r\n$5\r\nb_key\r\n$5\r\na_key\r\n$5\r\nd_key\r\n$5\r\nc_key\r\n"));
            }

            SECTION("Match - everything") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"SCAN", "0", "MATCH", "*"},
                        "*2\r\n:0\r\n*5\r\n$7\r\nkey_zzz\r\n$5\r\nb_key\r\n$5\r\na_key\r\n$5\r\nd_key\r\n$5\r\nc_key\r\n"));
            }
        }
    }
}
