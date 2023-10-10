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
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "config.h"
#include "fiber/fiber.h"
#include "fiber/fiber_scheduler.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker_fiber.h"
#include "signal_handler_thread.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "epoch_gc.h"
#include "epoch_gc_worker.h"

#include "program.h"

#include "test-modules-redis-command-fixture.hpp"

#pragma GCC diagnostic ignored "-Wwrite-strings"

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - BGSAVE", "[redis][command][BGSAVE]") {
    SECTION("Snapshot not running") {
        uint64_t iteration = worker_context->db->snapshot.iteration;

        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"BGSAVE"},
                "+OK\r\n"));

        // Wait up to 1s for the snapshot to be executed
        uint64_t now = clock_monotonic_int64_ms();
        do {
            // Check that the save operation has successfully run
            MEMORY_FENCE_LOAD();
            if (worker_context->db->snapshot.iteration > iteration) {
                break;
            }

            usleep(500);
        } while (clock_monotonic_int64_ms() - now < 1000);

        // Check that the save operation has successfully run
        REQUIRE(worker_context->db->snapshot.iteration > iteration);
    }
}
