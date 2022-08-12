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

#include "clock.h"
#include "exttypes.h"
#include "support/simple_file_io.h"
#include "pidfile.h"
#include "xalloc.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"
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
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"

#include "program.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

#define PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(WORKER_CONTEXT, RUNNING) { \
    do { \
        sched_yield(); \
        usleep(10000); \
        MEMORY_FENCE_LOAD(); \
    } while((WORKER_CONTEXT)->running == !RUNNING); \
}

TEST_CASE("program.c-redis-commands", "[program-redis-commands]") {
    worker_context_t *worker_context;
    volatile bool terminate_event_loop = false;
    char* cpus[] = { "0" };

    config_module_network_binding_t config_module_network_binding = {
            .host = "127.0.0.1",
            .port = 12345,
    };
    config_module_redis_t config_module_redis = {
            .max_key_length = 256,
            .max_command_length = 8 * 1024,
            .max_command_arguments = 128,
    };
    config_module_network_timeout_t config_module_network_timeout = {
            .read_ms = 1000,
            .write_ms = 1000,
    };
    config_module_network_t config_module_network = {
            .timeout = &config_module_network_timeout,
            .bindings = &config_module_network_binding,
            .bindings_count = 1,
    };
    config_module_t config_module = {
            .type = CONFIG_MODULE_TYPE_REDIS,
            .network = &config_module_network,
            .redis = &config_module_redis,
    };
    config_network_t config_network = {
            .backend = CONFIG_NETWORK_BACKEND_IO_URING,
            .max_clients = 10,
            .listen_backlog = 10,
    };
    config_database_t config_database = {
            .max_keys = 1000,
            .backend = CONFIG_DATABASE_BACKEND_MEMORY,
    };
    config_t config = {
            .cpus = cpus,
            .cpus_count = 1,
            .workers_per_cpus = 1,
            .network = &config_network,
            .modules = &config_module,
            .modules_count = 1,
            .database = &config_database,
    };
    uint32_t workers_count = config.cpus_count * config.workers_per_cpus;

    storage_db_config_t *db_config = storage_db_config_new();
    db_config->backend_type = STORAGE_DB_BACKEND_TYPE_MEMORY;
    db_config->max_keys = 1000;

    storage_db_t *db = storage_db_new(db_config, workers_count);
    storage_db_open(db);

    program_context_t *program_context = program_get_context();
    program_context->config = &config;
    program_context->db = db;

    program_config_thread_affinity_set_selected_cpus(program_context);
    program_workers_initialize_count(program_context);
    worker_context = program_workers_initialize_context(
            &terminate_event_loop,
            program_context);

    PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(worker_context, true);

    struct sockaddr_in address = {0};
    size_t buffer_send_data_len;
    char buffer_send[16 * 1024] = {0};
    char buffer_recv[16 * 1024] = {0};

    int clientfd = network_io_common_socket_tcp4_new(0);
    address.sin_family = AF_INET;
    address.sin_port = htons(config_module_network_binding.port);
    address.sin_addr.s_addr = inet_addr(config_module_network_binding.host);

    REQUIRE(connect(clientfd, (struct sockaddr *) &address, sizeof(address)) == 0);

    SECTION("Redis - command - generic tests") {
        SECTION("Unknown / unsupported command") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*1\r\n$15\r\nUNKNOWN COMMAND\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 54);
            REQUIRE(strncmp(buffer_recv, "-ERR unknown command `UNKNOWN COMMAND` with `0` args\r\n",
                            strlen("-ERR unknown command `UNKNOWN COMMAND` with `0` args\r\n")) == 0);
        }

        SECTION("Malformed - more data than declared") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*1\r\n$5\r\nUNKNOWN COMMAND\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 24);
            REQUIRE(strncmp(buffer_recv, "-ERR parsing error <8>\r\n",
                            strlen("-ERR parsing error <8>\r\n")) == 0);
        }

        SECTION("Timeout") {
            // Wait the read timeout plus 250ms
            usleep((config.modules[0].network->timeout->read_ms * 1000) + (250 * 1000));

            // The socket should be closed so recv should return 0
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 0);
        }

        SECTION("Too long") {
            int cmd_length = (int) config.modules[0].redis->max_command_length + 1;
            char expected_error[256] = {0};

            sprintf(
                    expected_error,
                    "-ERR the command length has exceeded <%u> bytes\r\n",
                    (int) config.modules[0].redis->max_command_length);
            snprintf(
                    buffer_send,
                    sizeof(buffer_send) - 1,
                    "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$%d\r\n%0*d\r\n",
                    cmd_length,
                    cmd_length,
                    0);
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == strlen(expected_error));
            REQUIRE(strncmp(buffer_recv, expected_error, strlen(expected_error)) == 0);
        }
    }

    SECTION("Redis - command - HELLO") {
        char *hello_v2_expected_response_start =
                "*14\r\n"
                "$6\r\nserver\r\n"
                "$17\r\ncachegrand-server\r\n"
                "$7\r\nversion\r\n"
                "$";
        char *hello_v2_expected_response_end =
                "\r\n"
                "$5\r\nproto\r\n"
                ":2\r\n"
                "$2\r\nid\r\n"
                ":0\r\n"
                "$4\r\nmode\r\n"
                "$10\r\nstandalone\r\n"
                "$4\r\nrole\r\n"
                "$6\r\nmaster\r\n"
                "$7\r\nmodules\r\n"
                "*0\r\n";

        char *hello_v3_expected_response_start =
                "%7\r\n"
                "$6\r\nserver\r\n$17\r\ncachegrand-server\r\n"
                "$7\r\nversion\r\n$";
        char *hello_v3_expected_response_end =
                "\r\n"
                "$5\r\nproto\r\n:3\r\n"
                "$2\r\nid\r\n:0\r\n"
                "$4\r\nmode\r\n$10\r\nstandalone\r\n"
                "$4\r\nrole\r\n$6\r\nmaster\r\n"
                "$7\r\nmodules\r\n*0\r\n";

        SECTION("HELLO - no version") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*1\r\n$5\r\nHELLO\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            size_t len = recv(clientfd, buffer_recv, sizeof(buffer_recv), 0);

            REQUIRE(len > strlen(hello_v2_expected_response_start) + strlen(hello_v2_expected_response_end));

            REQUIRE(strncmp(buffer_recv, hello_v2_expected_response_start, strlen(hello_v2_expected_response_start)) == 0);
            REQUIRE(strncmp(
                    buffer_recv + (len - strlen(hello_v2_expected_response_end)),
                    hello_v2_expected_response_end,
                    strlen(hello_v2_expected_response_end)) == 0);
        }

        SECTION("HELLO - v2") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*2\r\n$5\r\nHELLO\r\n$1\r\n2\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            size_t len = recv(clientfd, buffer_recv, sizeof(buffer_recv), 0);

            REQUIRE(len > strlen(hello_v2_expected_response_start) + strlen(hello_v2_expected_response_end));

            REQUIRE(strncmp(buffer_recv, hello_v2_expected_response_start, strlen(hello_v2_expected_response_start)) == 0);
            REQUIRE(strncmp(
                    buffer_recv + (len - strlen(hello_v2_expected_response_end)),
                    hello_v2_expected_response_end,
                    strlen(hello_v2_expected_response_end)) == 0);
        }

        SECTION("HELLO - v3") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            size_t len = recv(clientfd, buffer_recv, sizeof(buffer_recv), 0);

            REQUIRE(len > strlen(hello_v3_expected_response_start) + strlen(hello_v3_expected_response_end));

            REQUIRE(strncmp(buffer_recv, hello_v3_expected_response_start, strlen(hello_v3_expected_response_start)) == 0);
            REQUIRE(strncmp(
                    buffer_recv + (len - strlen(hello_v3_expected_response_end)),
                    hello_v3_expected_response_end,
                    strlen(hello_v3_expected_response_end)) == 0);
        }
    }

    SECTION("Redis - command - SET") {
        char *cmd_buffer_1 = "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$7\r\nb_value\r\n";
        char *cmd_buffer_2 = "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$7\r\nvalue_z\r\n";
        char *cmd_buffer_3 = "*3\r\n$3\r\nSET\r\n$40\r\nthis is a long key that can't be inlined\r\n$7\r\nb_value\r\n";

        SECTION("New key - short") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "%s", cmd_buffer_1);
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 5);
            REQUIRE(strncmp(buffer_recv, "+OK\r\n", strlen("+OK\r\n")) == 0);

            // TODO: check the hashtable
        }

        SECTION("New key - long") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "%s", cmd_buffer_3);
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0));
            REQUIRE(strncmp(buffer_recv, "+OK\r\n", strlen("+OK\r\n")) == 0);

            // TODO: check the hashtable
        }

        SECTION("Overwrite key") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "%s", cmd_buffer_1);
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 5);
            REQUIRE(strncmp(buffer_recv, "+OK\r\n", strlen("+OK\r\n")) == 0);

            snprintf(buffer_send, sizeof(buffer_send) - 1, "%s", cmd_buffer_2);
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 5);
            REQUIRE(strncmp(buffer_recv, "+OK\r\n", strlen("+OK\r\n")) == 0);

            // TODO: check the hashtable
        }

        SECTION("Missing parameters - key and value") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*1\r\n$3\r\nSET\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 50);
            REQUIRE(strncmp(
                    buffer_recv,
                    "-ERR wrong number of arguments for 'SET' command\r\n",
                    strlen("-ERR wrong number of arguments for 'SET' command\r\n")) == 0);
        }

        SECTION("Missing parameters - value") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*2\r\n$3\r\nSET\r\n$5\r\na_key\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 50);
            REQUIRE(strncmp(
                    buffer_recv,
                    "-ERR wrong number of arguments for 'SET' command\r\n",
                    strlen("-ERR wrong number of arguments for 'SET' command\r\n")) == 0);
        }

        SECTION("Too many parameters - one extra parameter") {
            snprintf(
                    buffer_send,
                    sizeof(buffer_send) - 1,
                    "*4\r\n$3\r\nSET\r\n$5\r\na_key\r\n$7\r\nb_value\r\n$15\r\nextra parameter\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 5);
            REQUIRE(strncmp(buffer_recv, "+OK\r\n", strlen("+OK\r\n")) == 0);
        }

        SECTION("Key too long") {
            int key_length = (int)config.modules[0].redis->max_key_length + 1;
            char expected_error[256] = { 0 };

            sprintf(
                    expected_error,
                    "-ERR The key has exceeded the allowed size of <%u>\r\n",
                    (int)config.modules[0].redis->max_key_length);
            snprintf(
                    buffer_send,
                    sizeof(buffer_send) - 1,
                    "*3\r\n$3\r\nSET\r\n$%d\r\n%0*d\r\n$7\r\na_value\r\n",
                    key_length,
                    key_length,
                    0);
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == strlen(expected_error));
            REQUIRE(strncmp(buffer_recv, expected_error, strlen(expected_error)) == 0);
        }
    }

    SECTION("Redis - command - DEL") {
        SECTION("Existing key") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$7\r\nb_value\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 5);
            REQUIRE(strncmp(buffer_recv, "+OK\r\n", strlen("+OK\r\n")) == 0);

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
            REQUIRE(strncmp(
                    buffer_recv,
                    "-ERR wrong number of arguments for 'DEL' command\r\n",
                    strlen("-ERR wrong number of arguments for 'DEL' command\r\n")) == 0);
        }

        SECTION("Key too long") {
            int key_length = (int)config.modules[0].redis->max_key_length + 1;
            char expected_error[256] = { 0 };

            sprintf(
                    expected_error,
                    "-ERR The key has exceeded the allowed size of <%u>\r\n",
                    (int)config.modules[0].redis->max_key_length);
            snprintf(
                    buffer_send,
                    sizeof(buffer_send) - 1,
                    "*2\r\n$3\r\nDEL\r\n$%d\r\n%0*d\r\n",
                    key_length,
                    key_length,
                    0);
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == strlen(expected_error));
            REQUIRE(strncmp(buffer_recv, expected_error, strlen(expected_error)) == 0);
        }
    }

    SECTION("Redis - command - GET") {
        SECTION("Existing key") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$7\r\nb_value\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 5);
            REQUIRE(strncmp(buffer_recv, "+OK\r\n", strlen("+OK\r\n")) == 0);

            snprintf(buffer_send, sizeof(buffer_send) - 1, "*2\r\n$3\r\nGET\r\n$5\r\na_key\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 13);
            REQUIRE(strncmp(buffer_recv, "$7\r\nb_value\r\n", strlen("$7\r\nb_value\r\n")) == 0);
        }

        SECTION("Existing key - pipelining") {
            char buffer_recv_expected[512] = { 0 };
            char *buffer_send_start = buffer_send;
            char *buffer_recv_expected_start = buffer_recv_expected;

            snprintf(buffer_send, sizeof(buffer_send) - 1, "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$7\r\nb_value\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 5);
            REQUIRE(strncmp(buffer_recv, "+OK\r\n", strlen("+OK\r\n")) == 0);

            for(int index = 0; index < 10; index++) {
                buffer_send_start += snprintf(
                        buffer_send_start,
                        sizeof(buffer_send) - (buffer_send_start - buffer_send) - 1,
                        "*2\r\n$3\r\nGET\r\n$5\r\na_key\r\n");

                buffer_recv_expected_start += snprintf(
                        buffer_recv_expected_start,
                        sizeof(buffer_recv_expected) - (buffer_recv_expected_start - buffer_recv_expected) - 1,
                        "$7\r\nb_value\r\n");
            }
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);

            size_t recv_len = 0;
            do {
                recv_len += recv(clientfd, buffer_recv, sizeof(buffer_recv), 0);
            } while(recv_len < 130);

            REQUIRE(strncmp(buffer_recv, buffer_recv_expected_start, strlen(buffer_recv_expected_start)) == 0);
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
            REQUIRE(strncmp(
                    buffer_recv,
                    "-ERR wrong number of arguments for 'GET' command\r\n",
                    strlen("-ERR wrong number of arguments for 'GET' command\r\n")) == 0);
        }

        SECTION("Key too long") {
            int key_length = (int)config.modules[0].redis->max_key_length + 1;
            char expected_error[256] = { 0 };

            sprintf(
                    expected_error,
                    "-ERR The key has exceeded the allowed size of <%u>\r\n",
                    (int)config.modules[0].redis->max_key_length);
            snprintf(
                    buffer_send,
                    sizeof(buffer_send) - 1,
                    "*2\r\n$3\r\nGET\r\n$%d\r\n%0*d\r\n",
                    key_length,
                    key_length,
                    0);
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == strlen(expected_error));
            REQUIRE(strncmp(buffer_recv, expected_error, strlen(expected_error)) == 0);
        }
    }

    SECTION("Redis - command - MGET") {
        SECTION("Existing key") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$7\r\nb_value\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 5);
            REQUIRE(strncmp(buffer_recv, "+OK\r\n", strlen("+OK\r\n")) == 0);

            snprintf(buffer_send, sizeof(buffer_send) - 1, "*2\r\n$4\r\nMGET\r\n$5\r\na_key\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 17);
            REQUIRE(strncmp(buffer_recv, "*1\r\n$7\r\nb_value\r\n", strlen("*1\r\n$7\r\nb_value\r\n")) == 0);
        }

        SECTION("Existing key - pipelining") {
            char buffer_recv_expected[512] = { 0 };
            char *buffer_send_start = buffer_send;
            char *buffer_recv_expected_start = buffer_recv_expected;

            snprintf(buffer_send, sizeof(buffer_send) - 1, "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$7\r\nb_value\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 5);
            REQUIRE(strncmp(buffer_recv, "+OK\r\n", strlen("+OK\r\n")) == 0);

            for(int index = 0; index < 10; index++) {
                buffer_send_start += snprintf(
                        buffer_send_start,
                        sizeof(buffer_send) - (buffer_send_start - buffer_send) - 1,
                        "*2\r\n$4\r\nMGET\r\n$5\r\na_key\r\n");

                buffer_recv_expected_start += snprintf(
                        buffer_recv_expected_start,
                        sizeof(buffer_recv_expected) - (buffer_recv_expected_start - buffer_recv_expected) - 1,
                        "$7\r\nb_value\r\n");
            }
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);

            size_t recv_len = 0;
            do {
                recv_len += recv(clientfd, buffer_recv, sizeof(buffer_recv), 0);
            } while(recv_len < 130);

            REQUIRE(strncmp(buffer_recv, buffer_recv_expected_start, strlen(buffer_recv_expected_start)) == 0);
        }

        SECTION("Non-existing key") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*2\r\n$4\r\nMGET\r\n$5\r\na_key\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 9);
            REQUIRE(strncmp(buffer_recv, "*1\r\n$-1\r\n", strlen("*1\r\n$-1\r\n")) == 0);
        }

        SECTION("Missing parameters - key") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*1\r\n$4\r\nMGET\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 51);
            REQUIRE(strncmp(
                    buffer_recv,
                    "-ERR wrong number of arguments for 'MGET' command\r\n",
                    strlen("-ERR wrong number of arguments for 'MGET' command\r\n")) == 0);
        }

        SECTION("Limit Arguments - Overflow") {
            off_t buffer_send_offset = 0;
            char expected_error[256] = { 0 };
            int key_count = (int)config.network->protocols->redis->max_command_arguments + 1;

            for(int key_index = 0; key_index < key_count; key_index++) {
                snprintf(buffer_send, sizeof(buffer_send) - 1, "*3\r\n$3\r\nSET\r\n$11\r\na_key_%05d\r\n$13\r\nb_value_%05d\r\n", key_index, key_index);
                buffer_send_data_len = strlen(buffer_send);

                REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
                REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 5);
                REQUIRE(strncmp(buffer_recv, "+OK\r\n", strlen("+OK\r\n")) == 0);
            }

            buffer_send_offset += snprintf(
                    buffer_send + buffer_send_offset,
                    sizeof(buffer_send) - buffer_send_offset - 1,
                    "*%d\r\n$4\r\nMGET\r\n",
                    key_count + 1);

            for(int key_index = 0; key_index < key_count; key_index++) {
                buffer_send_offset += snprintf(
                        buffer_send + buffer_send_offset,
                        sizeof(buffer_send) - buffer_send_offset - 1,
                        "$11\r\na_key_%05d\r\n",
                        key_index);
            }

            buffer_send_data_len = strlen(buffer_send);

            sprintf(
                    expected_error,
                    "-ERR command has too many arguments, the limit is <%u>\r\n",
                    (int)config.network->protocols->redis->max_command_arguments);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == strlen(expected_error));
            REQUIRE(strncmp(buffer_recv, expected_error, strlen(expected_error)) == 0);
        }

        SECTION("Key too long") {
            int key_length = (int)config.modules[0].redis->max_key_length + 1;
            char expected_error[256] = { 0 };

            sprintf(
                    expected_error,
                    "-ERR The key has exceeded the allowed size of <%u>\r\n",
                    (int)config.modules[0].redis->max_key_length);
            snprintf(
                    buffer_send,
                    sizeof(buffer_send) - 1,
                    "*2\r\n$4\r\nMGET\r\n$%d\r\n%0*d\r\n",
                    key_length,
                    key_length,
                    0);
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == strlen(expected_error));
            REQUIRE(strncmp(buffer_recv, expected_error, strlen(expected_error)) == 0);
        }

        SECTION("Fetch 2 keys") {
            int key_count = 2;
            char buffer_recv_cmp[8 * 1024] = { 0 };
            size_t buffer_recv_cmp_length;
            off_t buffer_recv_cmp_offset = 0;
            off_t buffer_send_offset = 0;

            for(int key_index = 0; key_index < key_count; key_index++) {
                snprintf(buffer_send, sizeof(buffer_send) - 1, "*3\r\n$3\r\nSET\r\n$11\r\na_key_%05d\r\n$13\r\nb_value_%05d\r\n", key_index, key_index);
                buffer_send_data_len = strlen(buffer_send);

                REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
                REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 5);
                REQUIRE(strncmp(buffer_recv, "+OK\r\n", strlen("+OK\r\n")) == 0);
            }

            buffer_send_offset += snprintf(
                    buffer_send + buffer_send_offset,
                    sizeof(buffer_send) - buffer_send_offset - 1,
                    "*3\r\n$4\r\nMGET\r\n");
            buffer_recv_cmp_offset += snprintf(
                    buffer_recv_cmp + buffer_recv_cmp_offset,
                    sizeof(buffer_recv_cmp) - buffer_recv_cmp_offset - 1,
                    "*2\r\n");

            for(int key_index = 0; key_index < key_count; key_index++) {
                buffer_send_offset += snprintf(
                        buffer_send + buffer_send_offset,
                        sizeof(buffer_send) - buffer_send_offset - 1,
                        "$11\r\na_key_%05d\r\n",
                        key_index);
                buffer_recv_cmp_offset += snprintf(
                        buffer_recv_cmp + buffer_recv_cmp_offset,
                        sizeof(buffer_recv_cmp) - buffer_recv_cmp_offset - 1,
                        "$13\r\nb_value_%05d\r\n",
                        key_index);
            }

            buffer_send_data_len = strlen(buffer_send);
            buffer_recv_cmp_length = strlen(buffer_recv_cmp);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == buffer_recv_cmp_length);
            REQUIRE(strncmp(buffer_recv, buffer_recv_cmp, buffer_recv_cmp_length) == 0);
        }

        SECTION("Fetch 128 keys") {
            int key_count = 128;
            char buffer_recv_cmp[8 * 1024] = { 0 };
            size_t buffer_recv_cmp_length;
            off_t buffer_recv_cmp_offset = 0;
            off_t buffer_send_offset = 0;

            for(int key_index = 0; key_index < key_count; key_index++) {
                snprintf(buffer_send, sizeof(buffer_send) - 1, "*3\r\n$3\r\nSET\r\n$11\r\na_key_%05d\r\n$13\r\nb_value_%05d\r\n", key_index, key_index);
                buffer_send_data_len = strlen(buffer_send);

                REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
                REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == 5);
                REQUIRE(strncmp(buffer_recv, "+OK\r\n", strlen("+OK\r\n")) == 0);
            }

            buffer_send_offset += snprintf(
                    buffer_send + buffer_send_offset,
                    sizeof(buffer_send) - buffer_send_offset - 1,
                    "*129\r\n$4\r\nMGET\r\n");
            buffer_recv_cmp_offset += snprintf(
                    buffer_recv_cmp + buffer_recv_cmp_offset,
                    sizeof(buffer_recv_cmp) - buffer_recv_cmp_offset - 1,
                    "*128\r\n");

            for(int key_index = 0; key_index < key_count; key_index++) {
                buffer_send_offset += snprintf(
                        buffer_send + buffer_send_offset,
                        sizeof(buffer_send) - buffer_send_offset - 1,
                        "$11\r\na_key_%05d\r\n",
                        key_index);
                buffer_recv_cmp_offset += snprintf(
                        buffer_recv_cmp + buffer_recv_cmp_offset,
                        sizeof(buffer_recv_cmp) - buffer_recv_cmp_offset - 1,
                        "$13\r\nb_value_%05d\r\n",
                        key_index);
            }

            buffer_send_data_len = strlen(buffer_send);
            buffer_recv_cmp_length = strlen(buffer_recv_cmp);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) == buffer_recv_cmp_length);
            REQUIRE(strncmp(buffer_recv, buffer_recv_cmp, buffer_recv_cmp_length) == 0);
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
    sched_yield();

    program_workers_cleanup(
            worker_context,
            1);

    REQUIRE(mprobe(worker_context) == -MCHECK_FREE);

    storage_db_close(db);
    storage_db_free(db, workers_count);

    program_reset_context();
}
