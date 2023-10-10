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
#include "transaction_rwspinlock.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "data_structures/double_linked_list/double_linked_list.h"
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
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "storage/db/storage_db.h"
#include "epoch_gc.h"
#include "epoch_gc_worker.h"
#include "module/redis/module_redis.h"

#include "program.h"

#include "test-modules-redis-command-fixture.hpp"

#pragma GCC diagnostic ignored "-Wwrite-strings"

void test_module_redis_command_hello_apply_template_expected_response(
        char *expected_response_template,
        char *buffer,
        size_t buffer_len) {
    snprintf(
            buffer,
            buffer_len,
            expected_response_template,
            strlen(MODULE_REDIS_COMPATIBILITY_SERVER_NAME), MODULE_REDIS_COMPATIBILITY_SERVER_NAME,
            strlen(MODULE_REDIS_COMPATIBILITY_SERVER_VERSION), MODULE_REDIS_COMPATIBILITY_SERVER_VERSION,
            strlen(CACHEGRAND_CMAKE_CONFIG_VERSION_GIT), CACHEGRAND_CMAKE_CONFIG_VERSION_GIT);
}


TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - HELLO", "[redis][command][HELLO]") {
    char hello_v2_expected_response[512] = { 0 };
    char hello_v3_expected_response[512] = { 0 };
    char *hello_v2_expected_response_template =
            "*16\r\n"
            "$6\r\nserver\r\n"
            "$%d\r\n%s\r\n"
            "$7\r\nversion\r\n"
            "$%d\r\n%s\r\n"
            "$18\r\ncachegrand_version\r\n"
            "$%d\r\n%s\r\n"
            "$5\r\nproto\r\n"
            ":2\r\n"
            "$2\r\nid\r\n"
            ":0\r\n"
            "$4\r\nmode\r\n"
            "$10\r\nstandalone\r\n"
            "$4\r\nrole\r\n"
            "$6\r\nmaster\r\n"
            "$7\r\nmodules\r\n"
            "*0\r\n";

    char *hello_v3_expected_response_template =
            "%8\r\n"
            "$6\r\nserver\r\n$%d\r\n%s\r\n"
            "$7\r\nversion\r\n$%d\r\n%s\r\n"
            "$18\r\ncachegrand_version\r\n$%d\r\n%s\r\n"
            "$5\r\nproto\r\n:3\r\n"
            "$2\r\nid\r\n:0\r\n"
            "$4\r\nmode\r\n$10\r\nstandalone\r\n"
            "$4\r\nrole\r\n$6\r\nmaster\r\n"
            "$7\r\nmodules\r\n*0\r\n";

    test_module_redis_command_hello_apply_template_expected_response(
            hello_v2_expected_response_template,
            hello_v2_expected_response,
            sizeof(hello_v2_expected_response));
    test_module_redis_command_hello_apply_template_expected_response(
            hello_v3_expected_response_template,
            hello_v3_expected_response,
            sizeof(hello_v3_expected_response));

    SECTION("HELLO - no version") {
        SECTION("without auth") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"HELLO"},
                    hello_v2_expected_response));
        }

        SECTION("with auth - default username") {
            config_module_redis.password = "apassword";
            config_module_redis.require_authentication = true;

            SECTION("fail - missing proto") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "AUTH", "default", config_module_redis.password},
                        "-NOPROTO unsupported protocol version\r\n"));
            }

            SECTION("fail - missing auth") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO"},
                        "-NOAUTH HELLO must be called with the client already authenticated, otherwise the HELLO AUTH <user> <pass> option can be used to authenticate the client and select the RESP protocol version at the same time\r\n"));
            }
        }
    }

    SECTION("HELLO - v2") {
        SECTION("without auth") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"HELLO", "2"},
                    hello_v2_expected_response));
        }

        SECTION("with auth - default username") {
            config_module_redis.password = "apassword";
            config_module_redis.require_authentication = true;

            SECTION("valid") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "2", "AUTH", "default", config_module_redis.password},
                        hello_v2_expected_response));
            }

            SECTION("wrong username") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "2", "AUTH", "wrongdefault", config_module_redis.password},
                        "-AUTH failed: WRONGPASS invalid username-password pair or user is disabled.\r\n"));
            }

            SECTION("wrong password") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "2", "AUTH", "default", "wrongpassword"},
                        "-AUTH failed: WRONGPASS invalid username-password pair or user is disabled.\r\n"));
            }

            SECTION("fail - missing auth") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "2"},
                        "-NOAUTH HELLO must be called with the client already authenticated, otherwise the HELLO AUTH <user> <pass> option can be used to authenticate the client and select the RESP protocol version at the same time\r\n"));
            }
        }

        SECTION("with auth - non default username") {
            config_module_redis.username = "nondefault";
            config_module_redis.password = "apassword";
            config_module_redis.require_authentication = true;

            SECTION("valid") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "2", "AUTH", config_module_redis.username, config_module_redis.password},
                        hello_v2_expected_response));
            }

            SECTION("wrong username") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "2", "AUTH", "wrongdefault", config_module_redis.password},
                        "-AUTH failed: WRONGPASS invalid username-password pair or user is disabled.\r\n"));
            }

            SECTION("wrong password") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "2", "AUTH", config_module_redis.username, "wrongpassword"},
                        "-AUTH failed: WRONGPASS invalid username-password pair or user is disabled.\r\n"));
            }
        }
    }

    SECTION("HELLO - v3") {
        SECTION("without auth") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"HELLO", "3"},
                    hello_v3_expected_response));
        }

        SECTION("with auth - default username") {
            config_module_redis.password = "apassword";
            config_module_redis.require_authentication = true;

            SECTION("valid") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "3", "AUTH", "default", config_module_redis.password},
                        hello_v3_expected_response));
            }

            SECTION("wrong username") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "3", "AUTH", "wrongdefault", config_module_redis.password},
                        "-AUTH failed: WRONGPASS invalid username-password pair or user is disabled.\r\n"));
            }

            SECTION("wrong password") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "3", "AUTH", "default", "wrongpassword"},
                        "-AUTH failed: WRONGPASS invalid username-password pair or user is disabled.\r\n"));
            }

            SECTION("fail - missing auth") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "3"},
                        "-NOAUTH HELLO must be called with the client already authenticated, otherwise the HELLO AUTH <user> <pass> option can be used to authenticate the client and select the RESP protocol version at the same time\r\n"));
            }
        }

        SECTION("with auth - non default username") {
            config_module_redis.username = "nondefault";
            config_module_redis.password = "apassword";
            config_module_redis.require_authentication = true;

            SECTION("valid") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "3", "AUTH", config_module_redis.username, config_module_redis.password},
                        hello_v3_expected_response));
            }

            SECTION("wrong username") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "3", "AUTH", "wrongdefault", config_module_redis.password},
                        "-AUTH failed: WRONGPASS invalid username-password pair or user is disabled.\r\n"));
            }

            SECTION("wrong password") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "3", "AUTH", config_module_redis.username, "wrongpassword"},
                        "-AUTH failed: WRONGPASS invalid username-password pair or user is disabled.\r\n"));
            }

            SECTION("already authenticated") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "3", "AUTH", config_module_redis.username, config_module_redis.password},
                        hello_v3_expected_response));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "3", "AUTH", config_module_redis.username, config_module_redis.password},
                        "-AUTH failed: already authenticated.\r\n"));
            }

            SECTION("wrong authentication after valid authentication") {
                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "3", "AUTH", config_module_redis.username, config_module_redis.password},
                        hello_v3_expected_response));

                REQUIRE(send_recv_resp_command_text_and_validate_recv(
                        std::vector<std::string>{"HELLO", "3", "AUTH", config_module_redis.username, "wrongpassword"},
                        "-AUTH failed: already authenticated.\r\n"));
            }
        }
    }
}
