/**
 * Copyright (C) 2018-2022 Vito Castellano
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

#include <unistd.h>
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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - PSETEX", "[redis][command][PSETEX]") {
    SECTION("Missing parameters - key, milliseconds and value") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"PSETEX"},
                "-ERR wrong number of arguments for 'psetex' command\r\n"));
    }

    SECTION("Missing parameters - value") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"PSETEX", "a_key", "100"},
                "-ERR wrong number of arguments for 'psetex' command\r\n"));
    }

    SECTION("Too many parameters - one extra parameter") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"PSETEX", "a_key", "100", "b_value", "extra parameter"},
                "-ERR wrong number of arguments for 'psetex' command\r\n"));
    }

    SECTION("Zero value as expire") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"PSETEX", "a_key", "0", "b_value"},
                "-ERR invalid expire time in 'psetex' command\r\n"));
    }

    SECTION("New key - expire in 500ms") {
        char *key = "a_key";
        char *value = "b_value";
        config_module_network_timeout.read_ms = 1000;

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"PSETEX", key, "500", value},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"GET", key},
                "$7\r\nb_value\r\n"));

        // Wait for 600 ms and try to get the value after the expiration
        usleep((500 + 100) * 1000);

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"GET", key},
                "$-1\r\n"));

        storage_db_entry_index_t *entry_index = storage_db_get_entry_index(db, key, strlen(key));
        REQUIRE(entry_index == NULL);
    }

    SECTION("New key - expire in 1s") {
        char *key = "a_key";
        char *value = "b_value";
        config_module_network_timeout.read_ms = 2000;

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"PSETEX", key, "1000", value},
                "+OK\r\n"));

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"GET", key},
                "$7\r\nb_value\r\n"));

        // Wait for 1100 ms and try to get the value after the expiration
        usleep((1000 + 100) * 1000);

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"GET", key},
                "$-1\r\n"));

        storage_db_entry_index_t *entry_index = storage_db_get_entry_index(db, key, strlen(key));
        REQUIRE(entry_index == NULL);
    }
}