/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
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
#include "storage/db/storage_db.h"
#include "epoch_gc.h"
#include "epoch_gc_worker.h"

#include "program.h"

#include "test-modules-redis-command-fixture.hpp"

#pragma GCC diagnostic ignored "-Wwrite-strings"

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - PING", "[redis][command][PING]") {
    SECTION("Without value - RESP") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"PING"},
                "+PONG\r\n"));
    }

    SECTION("With value - RESP") {
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"PING", "a test"},
                "$6\r\na test\r\n"));
    }

    SECTION("Without value - Inline") {
        this->protocol_version = TEST_MODULES_REDIS_COMMAND_FIXTURE_PROTOCOL_INLINE;

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"PING"},
                "+PONG\r\n"));
    }
}
