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

#include <unistd.h>
#include <netinet/in.h>

#include "clock.h"
#include "exttypes.h"
#include "memory_fences.h"
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

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - SHUTDOWN", "[redis][command][SHUTDOWN]") {
    REQUIRE(send_recv_resp_command_text_and_validate_recv(
            std::vector<std::string>{"SHUTDOWN"},
            "+OK\r\n"));

    // Wait 5 seconds in addition to the max duration of the wait time in the loop to ensure that the worker has
    // plenty of time to shut-down
    usleep((5000 + (WORKER_LOOP_MAX_WAIT_TIME_MS + 100)) * 1000);
    MEMORY_FENCE_LOAD();
    REQUIRE(!worker_context->running);
}
