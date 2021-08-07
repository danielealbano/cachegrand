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
#include "xalloc.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "config.h"
#include "worker/worker_common.h"
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
    config_network_protocol_binding_t config_network_protocol_binding = { \
            .host = "127.0.0.1", \
            .port = 12345, \
    }; \
    config_network_protocol_t config_network_protocol = { \
            .type = CONFIG_PROTOCOL_TYPE_REDIS, \
            .bindings = &config_network_protocol_binding, \
            .bindings_count = 1, \
    }; \
    config_network_t config_network = { \
            .backend = CONFIG_NETWORK_BACKEND_IO_URING,         \
            .max_clients = 10, \
            .listen_backlog = 10, \
            .protocols = &config_network_protocol, \
            .protocols_count = 1, \
    }; \
    config_storage_t config_storage = { \
            .backend = CONFIG_STORAGE_BACKEND_MEMORY, \
            .max_shard_size_mb = 50, \
    }; \
    config_database_t config_database = { \
            .max_keys = 1000, \
    }; \
    config_t config = { \
            .cpus = cpus, \
            .cpus_count = 1, \
            .workers_per_cpus = 1, \
            .network = &config_network, \
            .storage = &config_storage, \
            .database = &config_database, \
    }; \
    program_context_t program_context = { \
            .config = &config \
    };

#define PROGRAM_WAIT_FOR_WORKER_RUNNING(WORKER_CONTEXT) { \
    do { \
        pthread_yield(); \
        usleep(10000); \
        MEMORY_FENCE_LOAD(); \
    } while(!(WORKER_CONTEXT)->running); \
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
        pthread_yield();

        REQUIRE(pthread_create(
                &pthread_terminate,
                NULL,
                test_program_wait_loop_terminate,
                (void*)&terminate_event_loop) == 0);

        usleep((WORKER_LOOP_MAX_WAIT_TIME_MS + 100) * 1000);
        pthread_yield();

        REQUIRE(pthread_join(pthread_terminate, NULL) == 0);

        REQUIRE(pthread_kill(pthread_wait, 0) == ESRCH);
        REQUIRE(pthread_join(pthread_wait, NULL) == 0);
    }

    SECTION("program_workers_initialize") {
        PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345();
        worker_context_t* worker_context;
        volatile bool terminate_event_loop = false;

        program_config_thread_affinity_set_selected_cpus(&program_context);
        worker_context = program_workers_initialize(
                &terminate_event_loop,
                &program_context);

        REQUIRE(worker_context != NULL);
        REQUIRE(worker_context->workers_count == 1);
        REQUIRE(worker_context->worker_index == 0);
        REQUIRE(worker_context->terminate_event_loop == &terminate_event_loop);
        REQUIRE(worker_context->config == &config);
        REQUIRE(worker_context->pthread != 0);

        PROGRAM_WAIT_FOR_WORKER_RUNNING(worker_context);

        // Terminate running thread
        terminate_event_loop = true;
        MEMORY_FENCE_STORE();

        // Wait for the thread to end
        usleep((WORKER_LOOP_MAX_WAIT_TIME_MS + 100) * 1000);
        pthread_yield();

        // Check if the worker terminated
        REQUIRE(pthread_kill(worker_context->pthread, 0) == ESRCH);

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
        worker_context = program_workers_initialize(
                &terminate_event_loop,
                &program_context);

        PROGRAM_WAIT_FOR_WORKER_RUNNING(worker_context);

        terminate_event_loop = true;
        MEMORY_FENCE_STORE();

        // Wait for the thread to end
        usleep((WORKER_LOOP_MAX_WAIT_TIME_MS + 100) * 1000);
        pthread_yield();

        program_workers_cleanup(
                worker_context,
                1);

        REQUIRE(pthread_kill(worker_context->pthread, 0) == ESRCH);

        REQUIRE(mprobe(worker_context) == -MCHECK_FREE);
    }

    SECTION("program_setup_ulimit_wrapper") {
        ulong current_value;
        struct rlimit limit;

        REQUIRE(getrlimit(RLIMIT_NOFILE, &limit) == 0);
        current_value = limit.rlim_cur;
        REQUIRE(program_setup_ulimit_wrapper(RLIMIT_NOFILE, current_value - 1));
        REQUIRE(getrlimit(RLIMIT_NOFILE, &limit) == 0);
        REQUIRE(limit.rlim_cur == current_value - 1);
        REQUIRE(limit.rlim_max == current_value - 1);
    }

    SECTION("program_setup_ulimit") {
        // The test changes on purpose the current limit to ensure that it's not going to match the one being set
        // by the code, and it sets it to current - 1 to avoid hitting system limits.
        struct rlimit limit;

        REQUIRE(getrlimit(RLIMIT_NOFILE, &limit) == 0);
        REQUIRE(program_setup_ulimit_wrapper(RLIMIT_NOFILE, limit.rlim_max - 1));

        REQUIRE(getrlimit(RLIMIT_MEMLOCK, &limit) == 0);
        REQUIRE(program_setup_ulimit_wrapper(RLIMIT_MEMLOCK, limit.rlim_max - 1));

        REQUIRE(program_setup_ulimit());

        REQUIRE(getrlimit(RLIMIT_NOFILE, &limit) == 0);
        REQUIRE(limit.rlim_cur == PROGRAM_ULIMIT_NOFILE);
        REQUIRE(limit.rlim_max == PROGRAM_ULIMIT_NOFILE);

        REQUIRE(getrlimit(RLIMIT_MEMLOCK, &limit) == 0);
        REQUIRE(limit.rlim_cur == PROGRAM_ULIMIT_MEMLOCK);
        REQUIRE(limit.rlim_max == PROGRAM_ULIMIT_MEMLOCK);
    }

    SECTION("test redis ping/pong") {
        PROGRAM_CONFIG_AND_CONTEXT_REDIS_LOCALHOST_12345();
        pthread_t worker_pthread;
        worker_context_t *worker_context;
        struct sockaddr_in address = {0};
        size_t buffer_send_data_len;
        char buffer_send[64] = {0};
        char buffer_recv[64] = {0};
        volatile bool terminate_event_loop = false;

        program_config_thread_affinity_set_selected_cpus(&program_context);
        worker_context = program_workers_initialize(
                &terminate_event_loop,
                &program_context);

        PROGRAM_WAIT_FOR_WORKER_RUNNING(worker_context);

        int clientfd = network_io_common_socket_tcp4_new(0);

        address.sin_family = AF_INET;
        address.sin_port = htons(config_network_protocol_binding.port);
        address.sin_addr.s_addr = inet_addr(config_network_protocol_binding.host);
        snprintf(buffer_send, sizeof(buffer_send) - 1, "*1\r\n$4\r\nPING\r\n");
        buffer_send_data_len = strlen(buffer_send);

        REQUIRE(connect(clientfd, (struct sockaddr *) &address, sizeof(address)) == 0);
        REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
        REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 10);
        REQUIRE(strncmp(buffer_recv, "$4\r\nPONG\r\n", strlen("$4\r\nPONG\r\n")) == 0);
        REQUIRE(network_io_common_socket_close(clientfd, false));

        terminate_event_loop = true;
        MEMORY_FENCE_STORE();

        // Wait for the thread to end
        usleep((WORKER_LOOP_MAX_WAIT_TIME_MS + 100) * 1000);
        pthread_yield();

        program_workers_cleanup(
                worker_context,
                1);

        REQUIRE(pthread_kill(worker_context->pthread, 0) == ESRCH);

        REQUIRE(mprobe(worker_context) == -MCHECK_FREE);
    }
}
