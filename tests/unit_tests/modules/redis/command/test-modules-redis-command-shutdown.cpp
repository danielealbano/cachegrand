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

#include <unistd.h>
#include <netinet/in.h>

#include "clock.h"
#include "exttypes.h"
#include "memory_fences.h"
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
#include "storage/db/storage_db.h"
#include "epoch_gc.h"
#include "epoch_gc_worker.h"

#include "program.h"

#include "test-modules-redis-command-fixture.hpp"

#pragma GCC diagnostic ignored "-Wwrite-strings"

void test_modules_redis_command_shutdown_wait_for_teardown(
        worker_context_t* worker_context) {
    // Wait up to 5 seconds for the worker to stop
    for(int i = 0; i < 5000; i++) {
        MEMORY_FENCE_LOAD();
        if(!worker_context->running) {
            break;
        }

        usleep(500);
    }

    MEMORY_FENCE_LOAD();
    REQUIRE(!worker_context->running);
}

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - SHUTDOWN", "[redis][command][SHUTDOWN]") {
    SECTION("Plain") {
        uint64_t iteration = worker_context->db->snapshot.iteration;

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SHUTDOWN"},
                "+OK\r\n"));

        test_modules_redis_command_shutdown_wait_for_teardown(worker_context);

        REQUIRE(worker_context->db->snapshot.iteration == iteration);
    }

    SECTION("Trigger save") {
        uint64_t iteration = worker_context->db->snapshot.iteration;

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SHUTDOWN", "SAVE"},
                "+OK\r\n"));

        test_modules_redis_command_shutdown_wait_for_teardown(worker_context);

        // Check that the save operation has successfully run
        REQUIRE(worker_context->db->snapshot.iteration == iteration + 1);
    }

    SECTION("snapshot_at_shutdown = true") {
        // Set the flag to true to trigger a save operation on shutdown
        program_context->db->config->snapshot.snapshot_at_shutdown = true;
        MEMORY_FENCE_STORE();

        MEMORY_FENCE_LOAD();
        uint64_t iteration = worker_context->db->snapshot.iteration;

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SHUTDOWN"},
                "+OK\r\n"));

        test_modules_redis_command_shutdown_wait_for_teardown(worker_context);

        // Check that the save operation has successfully run
        REQUIRE(worker_context->db->snapshot.iteration == iteration + 1);
    }

    SECTION("Force nosave") {
        // To emulate the save operation we need to set the snapshot_at_shutdown flag
        program_context->db->config->snapshot.snapshot_at_shutdown = true;
        MEMORY_FENCE_STORE();

        MEMORY_FENCE_LOAD();
        uint64_t iteration = worker_context->db->snapshot.iteration;

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SHUTDOWN", "NOSAVE"},
                "+OK\r\n"));

        test_modules_redis_command_shutdown_wait_for_teardown(worker_context);

        // Check that the save operation has successfully run
        REQUIRE(worker_context->db->snapshot.iteration == iteration);
    }
}
