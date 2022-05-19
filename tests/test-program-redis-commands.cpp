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
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "config.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "signal_handler_thread.h"

#include "program.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

#define PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(WORKER_CONTEXT, RUNNING) { \
    do { \
        pthread_yield(); \
        usleep(10000); \
        MEMORY_FENCE_LOAD(); \
    } while((WORKER_CONTEXT)->running == !RUNNING); \
}

TEST_CASE("program.c-redis-commands", "[program-redis-commands]") {
    worker_context_t *worker_context;
    volatile bool terminate_event_loop = false;
    char* cpus[] = { "0" };
    config_network_protocol_binding_t config_network_protocol_binding = {
            .host = "127.0.0.1",
            .port = 12345,
    };
    config_network_protocol_t config_network_protocol = {
            .type = CONFIG_PROTOCOL_TYPE_REDIS,
            .bindings = &config_network_protocol_binding,
            .bindings_count = 1,
    };
    config_network_t config_network = {
            .backend = CONFIG_NETWORK_BACKEND_IO_URING,
            .max_clients = 10,
            .listen_backlog = 10,
            .protocols = &config_network_protocol,
            .protocols_count = 1,
    };
    config_storage_t config_storage = {
            .backend = CONFIG_STORAGE_BACKEND_IO_URING_FILE,
            .shard_size_mb = 50,
    };
    config_database_t config_database = {
            .max_keys = 1000,
    };
    config_t config = {
            .cpus = cpus,
            .cpus_count = 1,
            .workers_per_cpus = 1,
            .network = &config_network,
            .storage = &config_storage,
            .database = &config_database,
    };

    hashtable_config_t* hashtable_config = hashtable_mcmp_config_init();
    hashtable_config->initial_size = config_database.max_keys;
    hashtable_config->can_auto_resize = false;
    hashtable_t* hashtable = hashtable_mcmp_init(hashtable_config);

    program_context_t program_context = {
            .config = &config,
            .hashtable = hashtable,
    };

    program_config_thread_affinity_set_selected_cpus(&program_context);
    worker_context = program_workers_initialize_context(
            &terminate_event_loop,
            &program_context);

    PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(worker_context, true);

    struct sockaddr_in address = {0};
    size_t buffer_send_data_len;
    char buffer_send[512] = {0};
    char buffer_recv[512] = {0};

    int clientfd = network_io_common_socket_tcp4_new(0);
    address.sin_family = AF_INET;
    address.sin_port = htons(config_network_protocol_binding.port);
    address.sin_addr.s_addr = inet_addr(config_network_protocol_binding.host);

    REQUIRE(connect(clientfd, (struct sockaddr *) &address, sizeof(address)) == 0);

    SECTION("Redis - command - HELLO") {
        char *hello_v2_expected_response_start = "*14\r\n$6\r\nserver\r\n$10\r\ncachegrand\r\n$7\r\nversion\r\n$";
        char *hello_v2_expected_response_end = "\r\n$5\r\nproto\r\n:2\r\n$2\r\nid\r\n:0\r\n$4\r\nmode\r\n$10\r\nstandalone\r\n$4\r\nrole\r\n$6\r\nmaster\r\n$7\r\nmodules\r\n*0\r\n";

        char *hello_v3_expected_response_start = "%7\r\n$6\r\nserver\r\n$10\r\ncachegrand\r\n$7\r\nversion\r\n$";
        char *hello_v3_expected_response_end = "\r\n$5\r\nproto\r\n:3\r\n$2\r\nid\r\n:0\r\n$4\r\nmode\r\n$10\r\nstandalone\r\n$4\r\nrole\r\n$6\r\nmaster\r\n$7\r\nmodules\r\n*0\r\n";

        SECTION("HELLO - no version") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*1\r\n$5\r\nHELLO\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            size_t len = recv(clientfd, buffer_recv, sizeof(buffer_recv), 0);

            REQUIRE(len > strlen(hello_v2_expected_response_start) + strlen(hello_v2_expected_response_end));

            REQUIRE(strncmp(buffer_recv, hello_v2_expected_response_start, strlen(hello_v2_expected_response_start)) == 0);
            REQUIRE(strncmp(buffer_recv + (len - strlen(hello_v2_expected_response_end)), hello_v2_expected_response_end, strlen(hello_v2_expected_response_end)) == 0);
        }

        SECTION("HELLO - v2") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*2\r\n$5\r\nHELLO\r\n$1\r\n2\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            size_t len = recv(clientfd, buffer_recv, sizeof(buffer_recv), 0);

            REQUIRE(len > strlen(hello_v2_expected_response_start) + strlen(hello_v2_expected_response_end));

            REQUIRE(strncmp(buffer_recv, hello_v2_expected_response_start, strlen(hello_v2_expected_response_start)) == 0);
            REQUIRE(strncmp(buffer_recv + (len - strlen(hello_v2_expected_response_end)), hello_v2_expected_response_end, strlen(hello_v2_expected_response_end)) == 0);
        }

        SECTION("HELLO - v3") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            size_t len = recv(clientfd, buffer_recv, sizeof(buffer_recv), 0);

            REQUIRE(len > strlen(hello_v3_expected_response_start) + strlen(hello_v3_expected_response_end));

            REQUIRE(strncmp(buffer_recv, hello_v3_expected_response_start, strlen(hello_v3_expected_response_start)) == 0);
            REQUIRE(strncmp(buffer_recv + (len - strlen(hello_v3_expected_response_end)), hello_v3_expected_response_end, strlen(hello_v3_expected_response_end)) == 0);
        }
    }

    SECTION("Redis - command - SET") {
        char *key_name = "a_key";
        char *key_value_1 = "b_value";
        char *key_value_2 = "value_z";
        char *cmd_buffer_1 = "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$7\r\nb_value\r\n";
        char *cmd_buffer_2 = "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$7\r\nvalue_z\r\n";

        SECTION("New key") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "%s", cmd_buffer_1);
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 8);
            REQUIRE(strncmp(buffer_recv, "$2\r\nOK\r\n", strlen("$2\r\nOK\r\n")) == 0);

            // TODO: check the hashtable
        }

        SECTION("Overwrite key") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "%s", cmd_buffer_1);
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 8);
            REQUIRE(strncmp(buffer_recv, "$2\r\nOK\r\n", strlen("$2\r\nOK\r\n")) == 0);

            snprintf(buffer_send, sizeof(buffer_send) - 1, "%s", cmd_buffer_2);
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 8);
            REQUIRE(strncmp(buffer_recv, "$2\r\nOK\r\n", strlen("$2\r\nOK\r\n")) == 0);

            // TODO: check the hashtable
        }

        SECTION("Missing parameters - key and value") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*1\r\n$3\r\nSET\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 50);
            REQUIRE(strncmp(buffer_recv, "-ERR wrong number of arguments for 'SET' command\r\n", strlen("-ERR wrong number of arguments for 'SET' command\r\n")) == 0);

        }

        SECTION("Missing parameters - value") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*2\r\n$3\r\nSET\r\n$5\r\na_key\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 50);
            REQUIRE(strncmp(buffer_recv, "-ERR wrong number of arguments for 'SET' command\r\n", strlen("-ERR wrong number of arguments for 'SET' command\r\n")) == 0);
        }

        // TODO: this test should fail but the current implementation accepts and ignores the extra parameters, needs fixing!
//        SECTION("Too many parameters - one extra parameter") {
//            snprintf(buffer_send, sizeof(buffer_send) - 1, "*4\r\n$3\r\nSET\r\n$5\r\na_key\r\n$7\r\nb_value\r\n$15\r\nextra parameter\r\n");
//            buffer_send_data_len = strlen(buffer_send);
//
//            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
//            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 8);
//            REQUIRE(strncmp(buffer_recv, "$2\r\nOK\r\n", strlen("$2\r\nOK\r\n")) == 0);
//        }
    }

    SECTION("Redis - command - DEL") {
        SECTION("Existing key") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$7\r\nb_value\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 8);
            REQUIRE(strncmp(buffer_recv, "$2\r\nOK\r\n", strlen("$2\r\nOK\r\n")) == 0);

            snprintf(buffer_send, sizeof(buffer_send) - 1, "*2\r\n$3\r\nDEL\r\n$5\r\na_key\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 4);
            REQUIRE(strncmp(buffer_recv, ":1\r\n", strlen(":1\r\n")) == 0);
        }

        SECTION("Non-existing key") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*2\r\n$3\r\nDEL\r\n$5\r\na_key\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 4);
            REQUIRE(strncmp(buffer_recv, ":0\r\n", strlen(":0\r\n")) == 0);
        }

        SECTION("Missing parameters - key") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*1\r\n$3\r\nDEL\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 50);
            REQUIRE(strncmp(buffer_recv, "-ERR wrong number of arguments for 'DEL' command\r\n", strlen("-ERR wrong number of arguments for 'DEL' command\r\n")) == 0);
        }
    }

    SECTION("Redis - command - GET") {
        SECTION("Existing key") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$7\r\nb_value\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 8);
            REQUIRE(strncmp(buffer_recv, "$2\r\nOK\r\n", strlen("$2\r\nOK\r\n")) == 0);

            snprintf(buffer_send, sizeof(buffer_send) - 1, "*2\r\n$3\r\nGET\r\n$5\r\na_key\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 13);
            REQUIRE(strncmp(buffer_recv, "$7\r\nb_value\r\n", strlen("$7\r\nb_value\r\n")) == 0);
        }

        SECTION("Non-existing key") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*2\r\n$3\r\nGET\r\n$5\r\na_key\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 5);
            REQUIRE(strncmp(buffer_recv, "$-1\r\n", strlen("$-1\r\n")) == 0);
        }

        SECTION("Missing parameters - key") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*1\r\n$3\r\nGET\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 50);
            REQUIRE(strncmp(buffer_recv, "-ERR wrong number of arguments for 'GET' command\r\n", strlen("-ERR wrong number of arguments for 'GET' command\r\n")) == 0);
        }
    }

    SECTION("Redis - command - PING") {
        snprintf(buffer_send, sizeof(buffer_send) - 1, "*1\r\n$4\r\nPING\r\n");
        buffer_send_data_len = strlen(buffer_send);

        REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
        REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 10);
        REQUIRE(strncmp(buffer_recv, "$4\r\nPONG\r\n", strlen("$4\r\nPONG\r\n")) == 0);
    }

    SECTION("Redis - command - QUIT") {
        snprintf(buffer_send, sizeof(buffer_send) - 1, "*1\r\n$4\r\nQUIT\r\n");
        buffer_send_data_len = strlen(buffer_send);

        REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
        REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 8);
        REQUIRE(strncmp(buffer_recv, "$2\r\nOK\r\n", strlen("$2\r\nOK\r\n")) == 0);
    }

    SECTION("Redis - command - SHUTDOWN") {
        snprintf(buffer_send, sizeof(buffer_send) - 1, "*1\r\n$8\r\nSHUTDOWN\r\n");
        buffer_send_data_len = strlen(buffer_send);

        REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
        REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 8);
        REQUIRE(strncmp(buffer_recv, "$2\r\nOK\r\n", strlen("$2\r\nOK\r\n")) == 0);

        // Wait 5 seconds in addition to the max duration of the wait time in the loop to ensure that the worker has
        // plenty of time to shutdown
        usleep((5000 + (WORKER_LOOP_MAX_WAIT_TIME_MS + 100)) * 1000);
        MEMORY_FENCE_LOAD();
        REQUIRE(!worker_context->running);
    }

    close(clientfd);

    terminate_event_loop = true;
    MEMORY_FENCE_STORE();

    // Wait for the thread to end
    if (worker_context->running) {
        PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(worker_context, false);
        usleep((WORKER_LOOP_MAX_WAIT_TIME_MS + 100) * 1000);
    }
    pthread_yield();

    program_workers_cleanup(
            worker_context,
            1);

    REQUIRE(pthread_kill(worker_context->pthread, 0) == ESRCH);

    REQUIRE(mprobe(worker_context) == -MCHECK_FREE);

    hashtable_mcmp_free(hashtable);
}
