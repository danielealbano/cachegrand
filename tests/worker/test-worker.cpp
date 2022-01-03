#include <catch2/catch.hpp>

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "exttypes.h"
#include "xalloc.h"
#include "log/log.h"
#include "spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "config.h"
#include "worker/worker_common.h"
#include "worker/worker.h"

TEST_CASE("worker/worker.c", "[worker][worker]") {
    SECTION("worker_publish_stats") {
        worker_stats_t worker_stats_cmp = {0};
        worker_stats_t worker_stats_new = {0};
        worker_stats_volatile_t worker_stats_public = {0};

        size_t struct_size = sizeof(worker_stats_new.network);
        for(size_t i = 0; i < struct_size; i++) {
            ((uint8_t*)&worker_stats_cmp.network)[i] = i + 1;
            ((uint8_t*)&worker_stats_new.network)[i] = i + 1;
        }
        REQUIRE(clock_gettime(CLOCK_MONOTONIC, (struct timespec*)&worker_stats_new.last_update_timestamp) == 0);
        worker_stats_new.last_update_timestamp.tv_sec -= 1;

        worker_publish_stats(
                &worker_stats_new,
                &worker_stats_public);

        REQUIRE(memcmp((char*)&worker_stats_cmp.network, ((char*)&worker_stats_public.network), struct_size) == 0);
        REQUIRE(worker_stats_new.network.received_packets_per_second == 0);
        REQUIRE(worker_stats_new.network.sent_packets_per_second == 0);
        REQUIRE(worker_stats_new.network.accepted_connections_per_second == 0);

        REQUIRE(worker_stats_public.last_update_timestamp.tv_nsec == worker_stats_new.last_update_timestamp.tv_nsec);
        REQUIRE(worker_stats_public.last_update_timestamp.tv_sec == worker_stats_new.last_update_timestamp.tv_sec);
    }

    SECTION("worker_should_publish_stats") {
        SECTION("should") {
            worker_stats_volatile_t stats = {0};

            REQUIRE(worker_should_publish_stats(&stats));
        }

        SECTION("should not") {
            worker_stats_volatile_t stats = {0};

            REQUIRE(clock_gettime(CLOCK_MONOTONIC, (struct timespec*)&stats.last_update_timestamp) == 0);

            stats.last_update_timestamp.tv_sec += 100;

            REQUIRE(!worker_should_publish_stats(&stats));
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
        hashtable_t hashtable = {};
        worker_context_t worker_user_data = {0};
        network_channel_address_t addresses[1] = {0};
        volatile bool terminate_event_loop = false;

        worker_setup_context(
                &worker_user_data,
                1,
                1,
                &terminate_event_loop,
                &config,
                &hashtable);

        REQUIRE(worker_user_data.workers_count == 1);
        REQUIRE(worker_user_data.worker_index == 1);
        REQUIRE(worker_user_data.terminate_event_loop == &terminate_event_loop);
        REQUIRE(worker_user_data.config == &config);
        REQUIRE(worker_user_data.hashtable == &hashtable);
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
