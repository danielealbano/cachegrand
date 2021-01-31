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

#include "exttypes.h"
#include "xalloc.h"
#include "memory_fences.h"
#include "protocols/redis/protocol_redis.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "worker/worker.h"
#include "program.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

void* test_program_wait_loop_wait(
        void* user_data) {
    volatile bool *terminate_event_loop = (volatile bool *)user_data;

    program_wait_loop(terminate_event_loop);

    return NULL;
}

void* test_program_wait_loop_terminate(
        void* user_data) {
    volatile bool *terminate_event_loop = (volatile bool *)user_data;

    program_request_terminate(terminate_event_loop);

    return NULL;
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

    SECTION("program_signal_handlers") {
        SECTION("supported signal") {
            bool *program_terminate_event_loop = program_get_terminate_event_loop();
            *program_terminate_event_loop = false;

            program_signal_handlers(SIGUSR1);

            REQUIRE(program_should_terminate(program_terminate_event_loop));
        }

        SECTION("ignored signal") {
            bool *program_terminate_event_loop = program_get_terminate_event_loop();
            *program_terminate_event_loop = false;

            program_signal_handlers(SIGCHLD);

            REQUIRE(!program_should_terminate(program_terminate_event_loop));
        }
    }

    SECTION("program_register_signal_handlers") {
        program_register_signal_handlers();

//        SECTION("supported signal") {
//            bool *program_terminate_event_loop = program_get_terminate_event_loop();
//            *program_terminate_event_loop = false;
//
//            REQUIRE(kill(0, SIGUSR1) == 0);
//
//            REQUIRE(program_should_terminate(program_terminate_event_loop));
//        }

        SECTION("ignored signal") {
            bool *program_terminate_event_loop = program_get_terminate_event_loop();
            *program_terminate_event_loop = false;

            REQUIRE(kill(0, SIGCHLD) == 0);

            REQUIRE(!program_should_terminate(program_terminate_event_loop));
        }
    }

    SECTION("program_wait_loop") {
        volatile bool terminate_event_loop = false;
        pthread_t pthread_wait, pthread_terminate;
        pthread_attr_t attr;

        bool *program_terminate_event_loop = program_get_terminate_event_loop();
        *program_terminate_event_loop = false;

        REQUIRE(pthread_attr_init(&attr) == 0);
        REQUIRE(pthread_create(&pthread_wait, &attr, test_program_wait_loop_wait, (void*)&terminate_event_loop) == 0);

        sleep(1);

        REQUIRE(pthread_create(&pthread_terminate, &attr, test_program_wait_loop_terminate, (void*)&terminate_event_loop) == 0);

        sleep(3);

        REQUIRE(pthread_kill(pthread_terminate, 0) == ESRCH);
        REQUIRE(pthread_kill(pthread_wait, 0) == ESRCH);

        REQUIRE(pthread_join(pthread_wait, NULL) == 0);
        REQUIRE(pthread_join(pthread_terminate, NULL) == 0);
    }

    SECTION("program_workers_initialize") {
        pthread_attr_t attr;
        worker_user_data_t* worker_user_data;
        network_channel_address_t addresses[] = { PROGRAM_NETWORK_ADDRESSES };
        volatile bool terminate_event_loop = false;

        worker_user_data = program_workers_initialize(
                &terminate_event_loop,
                &attr,
                1);

        // TODO: the tests should be able to define on which addresses the program is running, multiple test runs can
        //       fail because of this
        REQUIRE(worker_user_data != NULL);
        REQUIRE(worker_user_data->worker_index == 0);
        REQUIRE(worker_user_data->terminate_event_loop == &terminate_event_loop);
        REQUIRE(worker_user_data->max_connections == PROGRAM_NETWORK_MAX_CONNECTIONS_PER_WORKER);
        REQUIRE(worker_user_data->backlog == PROGRAM_NETWORK_CONNECTIONS_BACKLOG);
        REQUIRE(worker_user_data->addresses_count == PROGRAM_NETWORK_ADDRESSES_COUNT);
        REQUIRE(worker_user_data->pthread != 0);

        sleep(1);

        // Terminate running thread
        terminate_event_loop = true;
        HASHTABLE_MEMORY_FENCE_STORE();

        // Wait for the thread to end
        sleep(3);

        // Check if the worker terminated
        REQUIRE(pthread_kill(worker_user_data->pthread, 0) == ESRCH);

        // Cleanup
        REQUIRE(pthread_join(worker_user_data->pthread, NULL) == 0);
        xalloc_free(worker_user_data);
    }

    SECTION("program_workers_cleanup") {
        pthread_t worker_pthread;
        pthread_attr_t attr;
        worker_user_data_t* worker_user_data;
        volatile bool terminate_event_loop = false;

        worker_user_data = program_workers_initialize(
                &terminate_event_loop,
                &attr,
                1);

        // Terminate running thread
        worker_pthread = worker_user_data->pthread;

        sleep(1);

        terminate_event_loop = true;
        HASHTABLE_MEMORY_FENCE_STORE();

        // Wait for the thread to end
        sleep(3);

        program_workers_cleanup(
                worker_user_data,
                1);

        REQUIRE(pthread_kill(worker_pthread, 0) == ESRCH);

        REQUIRE(mprobe(worker_user_data) == -MCHECK_FREE);
    }
//
//    SECTION("test redis ping/pong") {
//        pthread_t worker_pthread;
//        pthread_attr_t attr;
//        worker_user_data_t* worker_user_data;
//        struct sockaddr_in address = {0};
//        size_t buffer_send_data_len;
//        char buffer_send[64] = {0};
//        char buffer_recv[64] = {0};
//        volatile bool terminate_event_loop = false;
//
//        worker_user_data = program_workers_initialize(
//                &terminate_event_loop,
//                &attr,
//                1);
//
//        // Terminate running thread
//        worker_pthread = worker_user_data->pthread;
//
//        sleep(1);
//
//        int clientfd = network_io_common_socket_tcp4_new(0);
//
//        address.sin_family = AF_INET;
//        address.sin_port = htons(12345);
//        address.sin_addr.s_addr = inet_addr("127.0.0.1");
//        snprintf(buffer_send, sizeof(buffer_send) - 1, "PING\r\n");
//        buffer_send_data_len = strlen(buffer_send);
//
//        REQUIRE(connect(clientfd, (struct sockaddr*)&address, sizeof(address)) == 0);
//        REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
//        REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 7);
//        REQUIRE(strncmp(buffer_recv, "+PONG\r\n", strlen("+PONG\r\n")) == 0);
//        REQUIRE(network_io_common_socket_close(clientfd, false));
//
//        terminate_event_loop = true;
//        HASHTABLE_MEMORY_FENCE_STORE();
//
//        // Wait for the thread to end
//        sleep(3);
//
//        program_workers_cleanup(
//                worker_user_data,
//                1);
//
//        REQUIRE(pthread_kill(worker_pthread, 0) == ESRCH);
//
//        REQUIRE(mprobe(worker_user_data) == -MCHECK_FREE);
//    }
}
