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
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <mcheck.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/resource.h>

#include "exttypes.h"
#include "support/simple_file_io.h"
#include "pidfile.h"
#include "xalloc.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "config.h"
#include "fiber.h"
#include "network/channel/network_channel.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "signal_handler_thread.h"

#include "program.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

void* test_program_wait_loop_wait(
        void* user_data) {
    bool_volatile_t *terminate_event_loop = (bool_volatile_t*)user_data;

    program_wait_loop(NULL, 0, terminate_event_loop);

    return NULL;
}

void* test_program_wait_loop_terminate(
        void* user_data) {
    volatile bool *terminate_event_loop = (volatile bool *)user_data;

    program_request_terminate(terminate_event_loop);

    return NULL;
}

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
    config_module_t config_module = { \
            .type = CONFIG_MODULE_TYPE_REDIS, \
            .network = &config_module_network, \
    }; \
    config_network_t config_network = { \
            .backend = CONFIG_NETWORK_BACKEND_IO_URING,         \
            .max_clients = 10, \
            .listen_backlog = 10, \
    }; \
    config_database_t config_database = { \
            .max_keys = 1000, \
            .backend = CONFIG_DATABASE_BACKEND_MEMORY, \
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
    program_context_t program_context = { \
            .config = &config \
    };

#define PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(WORKER_CONTEXT, RUNNING) { \
    do { \
        sched_yield(); \
        usleep(10000); \
        MEMORY_FENCE_LOAD(); \
    } while((WORKER_CONTEXT)->running == !RUNNING); \
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

    SECTION("program_get_terminate_event_loop") {
        SECTION("false") {
            REQUIRE(!*program_get_terminate_event_loop());
        }

        SECTION("true") {
            bool *program_terminate_event_loop = program_get_terminate_event_loop();
            program_request_terminate(program_terminate_event_loop);
            REQUIRE(*program_get_terminate_event_loop());
        }
    }

    SECTION("program_setup_pidfile") {
        char fixture_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
        int fixture_temp_path_suffix_len = 4;
        close(mkstemps(fixture_temp_path, fixture_temp_path_suffix_len));

        SECTION("valid pidfile path") {
            PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345();

            program_context.config->pidfile_path = fixture_temp_path;

            REQUIRE(program_setup_pidfile(&program_context));

            REQUIRE(pidfile_get_fd() > -1);
            REQUIRE(pidfile_is_owned());
            REQUIRE(simple_file_io_read_uint32_return(program_context.config->pidfile_path) == (long)getpid());

            pidfile_close(pidfile_get_fd());
        }

        SECTION("valid pidfile path cleanup") {
            PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345();

            program_context.config->pidfile_path = fixture_temp_path;

            REQUIRE(program_setup_pidfile(&program_context));

            // Has to be set back to null otherwise when cyaml will try to free up the memory will trigger a segfault
            program_context.config = NULL;

            program_cleanup(&program_context);

            REQUIRE(pidfile_get_fd() == -1);
            REQUIRE(!pidfile_is_owned());
        }

        SECTION("null pidfile path") {
            PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345();

            REQUIRE(program_setup_pidfile(&program_context));

            REQUIRE(pidfile_get_fd() == -1);
            REQUIRE(!pidfile_is_owned());
        }

        unlink(fixture_temp_path);
    }

    SECTION("program_wait_loop") {
        volatile bool terminate_event_loop = false;
        pthread_t pthread_wait, pthread_terminate;

        bool *program_terminate_event_loop = program_get_terminate_event_loop();
        *program_terminate_event_loop = false;

        REQUIRE(pthread_create(
                &pthread_wait,
                NULL,
                test_program_wait_loop_wait,
                (void*)&terminate_event_loop) == 0);

        usleep(25000);
        sched_yield();

        REQUIRE(pthread_create(
                &pthread_terminate,
                NULL,
                test_program_wait_loop_terminate,
                (void*)&terminate_event_loop) == 0);

        usleep((WORKER_LOOP_MAX_WAIT_TIME_MS + 100) * 1000);
        sched_yield();

        REQUIRE(pthread_join(pthread_terminate, NULL) == 0);

        REQUIRE(pthread_join(pthread_wait, NULL) == 0);
    }

    SECTION("program_workers_initialize_count") {
        PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345();
        program_config_thread_affinity_set_selected_cpus(&program_context);
        program_workers_initialize_count(&program_context);
        REQUIRE(program_context.workers_count == 1);
    }

    SECTION("program_workers_initialize_context") {
        PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345();
        worker_context_t* worker_context;
        volatile bool terminate_event_loop = false;

        program_config_thread_affinity_set_selected_cpus(&program_context);
        program_workers_initialize_count(&program_context);
        worker_context = program_workers_initialize_context(
                &terminate_event_loop,
                &program_context);

        REQUIRE(worker_context != NULL);
        REQUIRE(worker_context->workers_count == 1);
        REQUIRE(worker_context->worker_index == 0);
        REQUIRE(worker_context->terminate_event_loop == &terminate_event_loop);
        REQUIRE(worker_context->config == &config);
        REQUIRE(worker_context->pthread != 0);

        PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(worker_context, true);

        // Terminate running thread
        terminate_event_loop = true;
        MEMORY_FENCE_STORE();

        // Wait for the thread to end
        PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(worker_context, false);
        usleep((WORKER_LOOP_MAX_WAIT_TIME_MS + 100) * 1000);
        sched_yield();

        // Cleanup
        REQUIRE(pthread_join(worker_context->pthread, NULL) == 0);
        xalloc_free(worker_context);
    }

    SECTION("program_workers_cleanup") {
        PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345();
        pthread_t worker_pthread;
        worker_context_t* worker_context;
        volatile bool terminate_event_loop = false;

        program_config_thread_affinity_set_selected_cpus(&program_context);
        program_workers_initialize_count(&program_context);
        worker_context = program_workers_initialize_context(
                &terminate_event_loop,
                &program_context);

        PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(worker_context, true);

        terminate_event_loop = true;
        MEMORY_FENCE_STORE();

        // Wait for the thread to end
        PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(worker_context, false);
        usleep((WORKER_LOOP_MAX_WAIT_TIME_MS + 100) * 1000);
        sched_yield();

        program_workers_cleanup(
                worker_context,
                1);

        REQUIRE(mprobe(worker_context) == -MCHECK_FREE);
    }
}
