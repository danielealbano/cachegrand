/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "clock.h"
#include "exttypes.h"
#include "xalloc.h"
#include "log/log.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "config.h"
#include "fiber/fiber.h"
#include "network/channel/network_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"

TEST_CASE("worker/worker.c", "[worker][worker]") {
    SECTION("worker_stats_publish") {
        size_t struct_size;
        worker_stats_t worker_stats_cmp = {0};
        worker_stats_t worker_stats_new = {0};
        worker_stats_volatile_t worker_stats_public = {0};

        struct_size = sizeof(worker_stats_new.network);
        for(size_t i = 0; i < struct_size; i++) {
            ((uint8_t*)&worker_stats_cmp.network)[i] = i + 1;
            ((uint8_t*)&worker_stats_new.network)[i] = i + 1;
        }

        struct_size = sizeof(worker_stats_new.storage);
        for(size_t i = 0; i < struct_size; i++) {
            ((uint8_t*)&worker_stats_cmp.storage)[i] = i + 1;
            ((uint8_t*)&worker_stats_new.storage)[i] = i + 1;
        }

        REQUIRE(clock_gettime(
                CLOCK_REALTIME,
                (struct timespec*)&worker_stats_new.total_last_update_timestamp) == 0);

        SECTION("total and per_minute") {
            worker_stats_publish(
                    &worker_stats_new,
                    &worker_stats_public,
                    false);

            REQUIRE(memcmp(
                    (char*)&worker_stats_new.network.total,
                    ((char*)&worker_stats_public.network.total),
                    sizeof(worker_stats_public.network.total)) == 0);
            REQUIRE(memcmp(
                    (char*)&worker_stats_new.storage.total,
                    ((char*)&worker_stats_public.storage.total),
                    sizeof(worker_stats_public.storage.total)) == 0);

            REQUIRE(memcmp(
                    (char*)&worker_stats_cmp.network.per_minute,
                    ((char*)&worker_stats_public.network.per_minute),
                    sizeof(worker_stats_public.network.per_minute)) == 0);
            REQUIRE(memcmp(
                    (char*)&worker_stats_cmp.storage.per_minute,
                    ((char*)&worker_stats_public.storage.per_minute),
                    sizeof(worker_stats_public.storage.per_minute)) == 0);

            REQUIRE(worker_stats_public.per_minute_last_update_timestamp.tv_nsec ==
                            worker_stats_public.total_last_update_timestamp.tv_nsec);
            REQUIRE(worker_stats_public.per_minute_last_update_timestamp.tv_sec ==
                            worker_stats_public.total_last_update_timestamp.tv_sec);

            REQUIRE(worker_stats_public.total_last_update_timestamp.tv_sec >=
                            worker_stats_new.total_last_update_timestamp.tv_sec);

            REQUIRE(worker_stats_public.started_on_timestamp.tv_nsec ==
                            worker_stats_new.started_on_timestamp.tv_nsec);
            REQUIRE(worker_stats_public.started_on_timestamp.tv_sec ==
                            worker_stats_new.started_on_timestamp.tv_sec);
        }

        SECTION("only total") {
            char zeroed_buffer[128] = { 0 };
            worker_stats_publish(
                    &worker_stats_new,
                    &worker_stats_public,
                    true);

            REQUIRE(memcmp(
                    (char*)&worker_stats_new.network.total,
                    ((char*)&worker_stats_public.network.total),
                    sizeof(worker_stats_public.network.total)) == 0);
            REQUIRE(memcmp(
                    (char*)&worker_stats_new.storage.total,
                    ((char*)&worker_stats_public.storage.total),
                    sizeof(worker_stats_public.storage.total)) == 0);

            REQUIRE(worker_stats_public.per_minute_last_update_timestamp.tv_nsec == 0);
            REQUIRE(worker_stats_public.per_minute_last_update_timestamp.tv_sec == 0);

            REQUIRE(worker_stats_public.total_last_update_timestamp.tv_sec >=
                    worker_stats_new.total_last_update_timestamp.tv_sec);

            REQUIRE(worker_stats_public.started_on_timestamp.tv_nsec ==
                    worker_stats_new.started_on_timestamp.tv_nsec);
            REQUIRE(worker_stats_public.started_on_timestamp.tv_sec ==
                    worker_stats_new.started_on_timestamp.tv_sec);

            // Ensure that all the per_minute related settings are set to zero
            REQUIRE(memcmp(
                    (char*)&worker_stats_public.network.per_minute,
                    zeroed_buffer,
                    sizeof(worker_stats_public.network.per_minute)) == 0);
            REQUIRE(memcmp(
                    (char*)&worker_stats_public.storage.per_minute,
                    zeroed_buffer,
                    sizeof(worker_stats_public.storage.per_minute)) == 0);
            REQUIRE(memcmp(
                    (char*)&worker_stats_public.per_minute_last_update_timestamp,
                    zeroed_buffer,
                    sizeof(worker_stats_public.per_minute_last_update_timestamp)) == 0);
        }
    }

    SECTION("worker_stats_should_publish_totals_after_interval") {
        SECTION("should") {
            worker_stats_volatile_t stats = {0};

            REQUIRE(worker_stats_should_publish_totals_after_interval(&stats));
        }

        SECTION("should not") {
            worker_stats_volatile_t stats = {0};

            clock_realtime((timespec_t*)&stats.total_last_update_timestamp);
            stats.total_last_update_timestamp.tv_sec += WORKER_PUBLISH_FULL_STATS_INTERVAL_SEC + 1;

            REQUIRE(!worker_stats_should_publish_totals_after_interval(&stats));
        }
    }

    SECTION("worker_stats_should_publish_per_minute_after_interval") {
        SECTION("should") {
            worker_stats_volatile_t stats = {0};

            REQUIRE(worker_stats_should_publish_per_minute_after_interval(&stats));
        }

        SECTION("should not") {
            worker_stats_volatile_t stats = {0};

            clock_realtime((timespec_t*)&stats.per_minute_last_update_timestamp);
            stats.per_minute_last_update_timestamp.tv_sec += WORKER_PUBLISH_FULL_STATS_INTERVAL_SEC + 1;

            REQUIRE(!worker_stats_should_publish_per_minute_after_interval(&stats));
        }
    }

    SECTION("worker_log_producer_set_early_prefix_thread") {
        worker_context_t worker_user_data = {0};
        char* str;

        str = worker_log_producer_set_early_prefix_thread(&worker_user_data);

        REQUIRE(str != NULL);
        REQUIRE(str == log_get_early_prefix_thread());

        log_unset_early_prefix_thread();
        xalloc_free(str);
    }

    SECTION("worker_setup_context") {
        config_t config = {};
        storage_db_t storage_db = { };
        worker_context_t worker_user_data = {0};
        network_channel_address_t addresses[1] = {0};
        timespec_t started_on_timestamp = { 0 };
        volatile bool terminate_event_loop = false;

        worker_setup_context(
                &worker_user_data,
                &started_on_timestamp,
                1,
                1,
                &terminate_event_loop,
                &config,
                &storage_db);

        REQUIRE(worker_user_data.workers_count == 1);
        REQUIRE(worker_user_data.worker_index == 1);
        REQUIRE(worker_user_data.terminate_event_loop == &terminate_event_loop);
        REQUIRE(worker_user_data.config == &config);
    }

    SECTION("worker_request_terminate") {
        volatile bool terminate_event_loop = false;
        worker_context_t worker_user_data = {0};
        worker_user_data.terminate_event_loop = &terminate_event_loop;

        worker_request_terminate(&worker_user_data);

        REQUIRE(terminate_event_loop);
        REQUIRE(*worker_user_data.terminate_event_loop);
    }

    SECTION("worker_should_terminate") {
        SECTION("should") {
            volatile bool terminate_event_loop = true;
            worker_context_t worker_user_data = {0};
            worker_user_data.terminate_event_loop = &terminate_event_loop;

            REQUIRE(worker_should_terminate(&worker_user_data));
        }

        SECTION("should not") {
            volatile bool terminate_event_loop = false;
            worker_context_t worker_user_data = {0};
            worker_user_data.terminate_event_loop = &terminate_event_loop;

            REQUIRE(!worker_should_terminate(&worker_user_data));
        }
    }

    SECTION("worker_set_running") {
        worker_context_t worker_context = { 0 };

        SECTION("running") {
            worker_context.running = false;

            worker_set_running(&worker_context, true);

            REQUIRE(worker_context.running == true);
        }

        SECTION("not running") {
            worker_context.running = true;

            worker_set_running(&worker_context, false);

            REQUIRE(worker_context.running == false);
        }
    }

    SECTION("worker_wait_running") {
        worker_context_t worker_context = { 0 };
        worker_context.running = true;

        // catch2 doesn't let to define timeouts, this function call should complete immediately
        worker_wait_running(&worker_context);

        REQUIRE(true);
    }
}
