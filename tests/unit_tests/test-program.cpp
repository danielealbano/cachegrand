/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include <cstdbool>
#include <pthread.h>
#include <mcheck.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>

#include "exttypes.h"
#include "support/simple_file_io.h"
#include "pidfile.h"
#include "clock.h"
#include "xalloc.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_rwspinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "config.h"
#include "fiber/fiber.h"
#include "network/channel/network_channel.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "signal_handler_thread.h"
#include "epoch_gc.h"
#include "epoch_gc_worker.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"

#include "program.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

void* test_program_wait_loop_wait(
        void* user_data) {
    auto *terminate_event_loop = (bool_volatile_t*)user_data;

    program_wait_loop(nullptr, 0, terminate_event_loop);

    return nullptr;
}

void* test_program_wait_loop_terminate(
        void* user_data) {
    volatile bool *terminate_event_loop = (volatile bool *)user_data;

    program_request_terminate(terminate_event_loop);

    return nullptr;
}

#define PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345_FREE() \
    storage_db_close(db); \
    storage_db_free(db, config.cpus_count);

#define PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345() \
     char* cpus[] = { "0" }; \
    config_module_network_binding_t config_module_network_binding = { \
            .host = "127.0.0.1", \
            .port = 12345, \
    }; \
    config_module_network_t config_module_network = { \
            .bindings = &config_module_network_binding, \
            .bindings_count = 1, \
    }; \
    config_module_redis_t config_module_redis = { \
    }; \
    config_module_t config_module = { \
            .type = "redis", \
            .module_id = module_get_by_name("redis")->id, \
            .network = &config_module_network, \
            .redis = &config_module_redis, \
    }; \
    config_network_t config_network = { \
            .backend = CONFIG_NETWORK_BACKEND_IO_URING,         \
            .max_clients = 10, \
            .listen_backlog = 10, \
    }; \
    config_database_limits_hard_t config_database_limits_hard = { \
            .max_keys = 1000, \
    }; \
    config_database_limits_t config_database_limits = { \
            .hard = &config_database_limits_hard, \
    }; \
     \
    config_database_memory_limits_hard_t config_database_memory_limits_hard = { \
            .max_memory_usage = 999999999999, \
    }; \
    config_database_memory_limits_t config_database_memory_limits = { \
            .hard = &config_database_memory_limits_hard \
    }; \
    config_database_memory_t config_database_memory = { \
            .limits = &config_database_memory_limits, \
    }; \
    config_database_keys_eviction_t config_database_keys_eviction = { \
            .only_ttl = false, \
            .policy = CONFIG_DATABASE_KEYS_EVICTION_POLICY_RANDOM \
    }; \
    config_database_t config_database = { \
            .limits = &config_database_limits, \
            .keys_eviction = &config_database_keys_eviction, \
            .backend = CONFIG_DATABASE_BACKEND_MEMORY, \
            .memory = &config_database_memory \
    }; \
    config_t config = { \
            .cpus = cpus, \
            .cpus_count = 1, \
            .workers_per_cpus = 1, \
            .network = &config_network, \
            .modules = &config_module, \
            .modules_count = 1, \
            .database = &config_database, \
    }; \
    \
    storage_db_config_t *db_config = storage_db_config_new(); \
    db_config->backend_type = STORAGE_DB_BACKEND_TYPE_MEMORY; \
    db_config->limits.keys_count.hard_limit = 1000; \
    \
    storage_db_t *db = storage_db_new(db_config, config.cpus_count); \
    storage_db_open(db); \
    \
    program_context_t program_context = { \
            .config = &config, \
            .db = db, \
    }; \

#define PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(WORKER_CONTEXT, RUNNING) { \
    do { \
        sched_yield(); \
        usleep(10000); \
        MEMORY_FENCE_LOAD(); \
    } while((WORKER_CONTEXT)->running == !(RUNNING)); \
}

TEST_CASE("program.c", "[program]") {
    SECTION("program_request_terminate") {
        volatile bool terminate_event_loop = false;
        program_request_terminate(&terminate_event_loop);

        REQUIRE(terminate_event_loop);
    }

    SECTION("program_should_terminate") {
        SECTION("should") {
            volatile bool terminate_event_loop = true;
            REQUIRE(program_should_terminate(&terminate_event_loop));
        }

        SECTION("should not") {
            volatile bool terminate_event_loop = false;
            REQUIRE(!program_should_terminate(&terminate_event_loop));
        }
    }

    SECTION("program_setup_pidfile") {
        char fixture_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
        int fixture_temp_path_suffix_len = 4;
        close(mkstemps(fixture_temp_path, fixture_temp_path_suffix_len));

        SECTION("valid pidfile path") {
            PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345()

            program_context.config->pidfile_path = fixture_temp_path;

            REQUIRE(program_setup_pidfile(&program_context));

            REQUIRE(pidfile_get_fd() > -1);
            REQUIRE(pidfile_is_owned());
            REQUIRE(simple_file_io_read_uint32_return(program_context.config->pidfile_path) == (long)getpid());

            pidfile_close(pidfile_get_fd());

            PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345_FREE()
        }

        SECTION("valid pidfile path cleanup") {
            PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345()

            program_context.config->pidfile_path = fixture_temp_path;

            REQUIRE(program_setup_pidfile(&program_context));

            // Has to be set back to null otherwise when cyaml will try to free up the memory will trigger a segfault
            program_context.config = nullptr;

            program_cleanup(&program_context);

            REQUIRE(pidfile_get_fd() == -1);
            REQUIRE(!pidfile_is_owned());
        }

        SECTION("null pidfile path") {
            PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345()

            REQUIRE(program_setup_pidfile(&program_context));

            REQUIRE(pidfile_get_fd() == -1);
            REQUIRE(!pidfile_is_owned());

            PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345_FREE()
        }

        unlink(fixture_temp_path);
    }

    SECTION("program_wait_loop") {
        volatile bool terminate_event_loop = false;
        pthread_t pthread_wait, pthread_terminate;

        REQUIRE(pthread_create(
                &pthread_wait,
                nullptr,
                test_program_wait_loop_wait,
                (void*)&terminate_event_loop) == 0);

        usleep(1000);
        sched_yield();

        REQUIRE(pthread_create(
                &pthread_terminate,
                nullptr,
                test_program_wait_loop_terminate,
                (void*)&terminate_event_loop) == 0);

        usleep(1000);
        sched_yield();

        REQUIRE(pthread_join(pthread_terminate, nullptr) == 0);
        REQUIRE(pthread_join(pthread_wait, nullptr) == 0);
    }

    SECTION("program_workers_initialize_count") {
        PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345()

        program_config_thread_affinity_set_selected_cpus(&program_context);
        program_workers_initialize_count(&program_context);
        REQUIRE(program_context.workers_count == 1);

        PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345_FREE()
    }

    SECTION("program_workers_initialize_context") {
        PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345()

        worker_context_t* worker_context;

        program_config_thread_affinity_set_selected_cpus(&program_context);
        program_workers_initialize_count(&program_context);
        worker_context = program_workers_initialize_context(
                &program_context);

        REQUIRE(worker_context != nullptr);
        REQUIRE(worker_context->workers_count == 1);
        REQUIRE(worker_context->worker_index == 0);
        REQUIRE(worker_context->terminate_event_loop == &program_context.workers_terminate_event_loop);
        REQUIRE(worker_context->config == &config);
        REQUIRE(worker_context->pthread != 0);

        PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(worker_context, true)

        // Terminate running thread
        program_context.workers_terminate_event_loop = true;
        MEMORY_FENCE_STORE();

        // Wait for the thread to end
        PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(worker_context, false)
        sched_yield();

        // Cleanup
        REQUIRE(pthread_join(worker_context->pthread, nullptr) == 0);
        xalloc_free(worker_context);

        PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345_FREE()
    }

    SECTION("program_workers_cleanup") {
        PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345()

        worker_context_t* worker_context;

        program_config_thread_affinity_set_selected_cpus(&program_context);
        program_workers_initialize_count(&program_context);
        worker_context = program_workers_initialize_context(
                &program_context);

        PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(worker_context, true)

        program_context.workers_terminate_event_loop = true;
        MEMORY_FENCE_STORE();

        // Wait for the thread to end
        PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(worker_context, false)
        sched_yield();

        program_workers_cleanup(
                worker_context,
                1);

        REQUIRE(mprobe(worker_context) == -MCHECK_FREE);

        PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345_FREE()
    }
}
