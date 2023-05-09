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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - AUTH", "[redis][command][AUTH]") {
    config_module_redis.password = "apassword";
    config_module_redis.require_authentication = true;

    SECTION("with default username") {
        SECTION("valid - without username") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", config_module_redis.password},
                    "+OK\r\n"));
        }

        SECTION("valid - with username") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", "default", config_module_redis.password},
                    "+OK\r\n"));
        }

        SECTION("wrong username") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", "wrongusername", config_module_redis.password},
                    "-AUTH failed: WRONGPASS invalid username-password pair or user is disabled.\r\n"));
        }

        SECTION("wrong password - without username") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", "wrongpassword"},
                    "-AUTH failed: WRONGPASS invalid username-password pair or user is disabled.\r\n"));
        }

        SECTION("wrong password - with username") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", config_module_redis.password, "wrongpassword"},
                    "-AUTH failed: WRONGPASS invalid username-password pair or user is disabled.\r\n"));
        }

        SECTION("double authentication") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", "default", config_module_redis.password},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", "default", config_module_redis.password},
                    "-AUTH failed: already authenticated.\r\n"));
        }

        SECTION("wrong authentication after valid authentication") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", "default", config_module_redis.password},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", "default", "wrongpassword"},
                    "-AUTH failed: already authenticated.\r\n"));
        }
    }

    SECTION("without default username") {
        config_module_redis.username = "anusername";

        SECTION("valid - without username") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", config_module_redis.password},
                    "+OK\r\n"));
        }

        SECTION("valid - with username") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", config_module_redis.username, config_module_redis.password},
                    "+OK\r\n"));
        }

        SECTION("wrong username") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", "wrongusername", config_module_redis.password},
                    "-AUTH failed: WRONGPASS invalid username-password pair or user is disabled.\r\n"));
        }

        SECTION("wrong password - without username") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", "wrongpassword"},
                    "-AUTH failed: WRONGPASS invalid username-password pair or user is disabled.\r\n"));
        }

        SECTION("wrong password - with username") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", config_module_redis.password, "wrongpassword"},
                    "-AUTH failed: WRONGPASS invalid username-password pair or user is disabled.\r\n"));
        }

        SECTION("double authentication") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", config_module_redis.username, config_module_redis.password},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", config_module_redis.username, config_module_redis.password},
                    "-AUTH failed: already authenticated.\r\n"));
        }

        SECTION("wrong authentication after valid authentication") {
            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", config_module_redis.username, config_module_redis.password},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text_and_validate_recv(
                    std::vector<std::string>{"AUTH", config_module_redis.username, "wrongpassword"},
                    "-AUTH failed: already authenticated.\r\n"));
        }
    }
}
