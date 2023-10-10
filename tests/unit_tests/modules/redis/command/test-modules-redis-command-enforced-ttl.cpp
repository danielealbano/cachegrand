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

#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>

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
#include "storage/db/storage_db.h"
#include "epoch_gc.h"
#include "epoch_gc_worker.h"

#include "program.h"

#include "test-modules-redis-command-fixture.hpp"

#pragma GCC diagnostic ignored "-Wwrite-strings"

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - Enforced TTL", "[redis][command][enforced_ttl]") {
    db->config->enforced_ttl.default_ms = 86400UL * 1000UL;
    db->config->enforced_ttl.max_ms = 7 * 86400UL * 1000UL;

    char *a_key = "a_key";
    char *a_value = "a_value";

    SECTION("No TTL - Use default") {
        int64_t before_timestamp = clock_realtime_coarse_int64_ms();
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SET", a_key, a_value},
                "+OK\r\n"));
        int64_t after_timestamp = clock_realtime_coarse_int64_ms();
        storage_db_entry_index_t *entry_index = storage_db_get_entry_index(
                db,
                0,
                a_key,
                strlen(a_key));

        REQUIRE(entry_index != nullptr);
        REQUIRE(entry_index->expiry_time_ms >= before_timestamp + db->config->enforced_ttl.default_ms);
        REQUIRE(entry_index->expiry_time_ms <= after_timestamp + db->config->enforced_ttl.default_ms);
    }

    SECTION("With TTL - 500ms") {
        int64_t before_timestamp = clock_realtime_coarse_int64_ms();
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SET", a_key, a_value, "PX", "500"},
                "+OK\r\n"));
        int64_t after_timestamp = clock_realtime_coarse_int64_ms();

        storage_db_entry_index_t *entry_index = storage_db_get_entry_index(
                db,
                0,
                a_key,
                strlen(a_key));

        REQUIRE(entry_index != nullptr);
        REQUIRE(entry_index->expiry_time_ms >= before_timestamp + 500 - 10);
        REQUIRE(entry_index->expiry_time_ms <= after_timestamp + 500 + 10);
    }

    SECTION("With TTL - 5s") {
        int64_t before_timestamp = clock_realtime_coarse_int64_ms();
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SET", a_key, a_value, "EX", "5"},
                "+OK\r\n"));
        int64_t after_timestamp = clock_realtime_coarse_int64_ms();

        storage_db_entry_index_t *entry_index = storage_db_get_entry_index(
                db,
                0,
                a_key,
                strlen(a_key));

        REQUIRE(entry_index != nullptr);
        REQUIRE(entry_index->expiry_time_ms >= before_timestamp + 5000 - 10);
        REQUIRE(entry_index->expiry_time_ms <= after_timestamp + 5000 + 10);
    }

    SECTION("With TTL - greater than max") {
        char greater_than_max_ttl[32];
        sprintf(greater_than_max_ttl, "%lu", db->config->enforced_ttl.max_ms + 1);

        int64_t before_timestamp = clock_realtime_coarse_int64_ms();
        REQUIRE(send_recv_resp_command_text_and_validate_recv(
                std::vector<std::string>{"SET", a_key, a_value, "PX", greater_than_max_ttl},
                "+OK\r\n"));
        int64_t after_timestamp = clock_realtime_coarse_int64_ms();

        storage_db_entry_index_t *entry_index = storage_db_get_entry_index(
                db,
                0,
                a_key,
                strlen(a_key));

        REQUIRE(entry_index != nullptr);
        REQUIRE(entry_index->expiry_time_ms >= before_timestamp + db->config->enforced_ttl.max_ms);
        REQUIRE(entry_index->expiry_time_ms <= after_timestamp + db->config->enforced_ttl.max_ms);
    }
}
