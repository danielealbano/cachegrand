/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include <cstdint>
#include <cstdbool>
#include <cstring>
#include <memory>
#include <string>
#include <stdexcept>
#include <stdarg.h>

#include <pthread.h>
#include <mcheck.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>

#include "random.h"
#include "clock.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"
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

int recv_packet_size = 32 * 1024;

template<typename ... Args>
std::string string_format( const std::string& format, Args ... args )
{
    int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    if( size_s <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
    auto size = static_cast<size_t>( size_s );
    std::unique_ptr<char[]> buf( new char[ size ] );
    std::snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}

#define PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(WORKER_CONTEXT, RUNNING) { \
    do { \
        sched_yield(); \
        usleep(10000); \
        MEMORY_FENCE_LOAD(); \
    } while((WORKER_CONTEXT)->running == !(RUNNING)); \
}

size_t build_resp_command(
        char *buffer,
        size_t buffer_size,
        const std::vector<std::string>& arguments) {
    size_t buffer_offset = 0;
    size_t arguments_count = arguments.size();

    buffer_offset += snprintf(
            buffer ? buffer + buffer_offset : nullptr,
            buffer ? buffer_size - buffer_offset : 0,
            "*%lu\r\n",
            arguments_count);

    for(const auto& value: arguments) {
        char *buffer_start = buffer + buffer_offset;
        buffer_offset += snprintf(
                buffer ? buffer_start : nullptr,
                buffer ? buffer_size - buffer_offset : 0,
                "$%lu\r\n%s\r\n",
                value.length(),
                value.c_str());
    }

    return buffer_offset;
}

size_t string_replace(
        char *input,
        size_t input_len,
        char *output,
        size_t output_len,
        int count,
        ...) {
    va_list arg_ptr;
    char *current_char = input;
    char *output_begin = output;

    char **replace_list = (char**)malloc(sizeof(char*) * count);
    size_t *replace_len_list = (size_t*)malloc(sizeof(size_t) * count);
    char **with_list = (char**)malloc(sizeof(char*) * count);
    size_t *with_len_list = (size_t*)malloc(sizeof(size_t) * count);

    int arg_index = 0;
    va_start(arg_ptr, count);
    while(arg_index < count) {
        replace_list[arg_index] = va_arg(arg_ptr, char *);
        replace_len_list[arg_index] = va_arg(arg_ptr, size_t);
        with_list[arg_index] = va_arg(arg_ptr, char *);
        with_len_list[arg_index] = va_arg(arg_ptr, size_t);

        arg_index++;
    }
    va_end(arg_ptr);

    while(current_char - input < input_len) {
        int replace_index_found = -1;

        for(int index = 0; index < count; index++) {
            if (*current_char != *(replace_list[index])) {
                continue;
            }

            if (memcmp(current_char, replace_list[index], replace_len_list[index]) != 0) {
                continue;
            }

            replace_index_found = index;
        }

        if (replace_index_found == -1) {
            *output = *current_char;
            output++;
            current_char++;
            continue;
        }

        memcpy(output, with_list[replace_index_found], with_len_list[replace_index_found]);

        output += with_len_list[replace_index_found];
        current_char += replace_len_list[replace_index_found];
    }

    *(output+1) = 0;

    return output - output_begin;
}

size_t send_recv_resp_command_calculate_multi_recv(
        size_t expected_length) {
    if (expected_length < recv_packet_size) {
        return 1;
    }

    return 1 + ceil((float)expected_length / (float)recv_packet_size) + 1;
}

bool send_recv_resp_command_multi_recv(
        int client_fd,
        const std::vector<std::string>& arguments,
        char *expected,
        size_t expected_length,
        int max_recv_count) {
    bool return_res = false;
    bool recv_matches_expected = false;
    size_t total_send_length, total_recv_length;

    // Allocates the memory to receive the response with a length up to n. of recv expected * max size of the packets
    size_t buffer_recv_length = (max_recv_count * recv_packet_size) + 1;
    char *buffer_recv = (char *)malloc(buffer_recv_length);
    memset(buffer_recv, 0, buffer_recv_length);

    assert(buffer_recv_length >= expected_length);

    // Build the resp command
    size_t buffer_send_length = build_resp_command(nullptr, 0, arguments);
    char *buffer_send = (char *) malloc(buffer_send_length + 1);
    build_resp_command(buffer_send, buffer_send_length + 1, arguments);

    // Send the data
    total_send_length = 0;
    do {
        size_t allowed_buffer_send_length =
                buffer_send_length - total_send_length > NETWORK_CHANNEL_MAX_PACKET_SIZE
                ? NETWORK_CHANNEL_MAX_PACKET_SIZE
                : buffer_send_length - total_send_length;

        assert(allowed_buffer_send_length > 0);
        assert(allowed_buffer_send_length <= NETWORK_CHANNEL_MAX_PACKET_SIZE);

        ssize_t send_length = send(client_fd, buffer_send + total_send_length, allowed_buffer_send_length, MSG_NOSIGNAL);
        if (send_length <= 0) {
            if (send_length < 0) {
                fprintf(stderr, "[ SEND - ERROR %d AFTER %lu BYTES SENT ]\n", errno, total_send_length);
                perror("[ SEND - ERROR ]");
            } else {
                fprintf(stderr, "[ SEND - SOCKET CLOSED ]\n");
            }

            fflush(stderr);
            return false;
        }

        total_send_length += send_length;
    } while(total_send_length < buffer_send_length);

    // To ensure that the test will not wait forever on some data because of a bug, recv_count has to point to the
    // number of expected packets will be received, although this is very implementation specific and can be broken
    // easily by a code change, this is part of a unit test, so it's fine
    total_recv_length = 0;
    for(int i = 0; i < max_recv_count && total_recv_length < expected_length; i++) {
        size_t allowed_buffer_recv_size = expected_length - total_recv_length;
        allowed_buffer_recv_size =
                allowed_buffer_recv_size < recv_packet_size
                ? recv_packet_size
                : allowed_buffer_recv_size;

        assert(allowed_buffer_recv_size > 0);

        ssize_t recv_length = recv(
                client_fd,
                buffer_recv + total_recv_length,
                allowed_buffer_recv_size,
                0);

        if (recv_length <= 0) {
            if (recv_length < 0) {
                fprintf(stderr, "[ RECV - ERROR %d AFTER %lu BYTES RECEIVED ]\n", errno, total_recv_length);
                perror("[ RECV - ERROR ]");
                return false;
            } else {
                fprintf(stderr, "[ RECV - SOCKET CLOSED ]\n");
                fflush(stderr);
                break;
            }
        }

        total_recv_length += recv_length;

        if (memcmp(buffer_recv, expected, total_recv_length) != 0) {
            break;
        }
    }

    recv_matches_expected = memcmp(buffer_recv, expected, total_recv_length) == 0;
    return_res = total_send_length == buffer_send_length && total_recv_length == expected_length && recv_matches_expected;

    if (!return_res) {
        char temp_buffer_1[1024] = { 0 };
        char temp_buffer_2[1024] = { 0 };

        memset(temp_buffer_1, 0, sizeof(temp_buffer_1));
        memset(temp_buffer_2, 0, sizeof(temp_buffer_2));
        string_replace(
                buffer_send, total_send_length > 64 ? 64 : total_send_length,
                temp_buffer_1, sizeof(temp_buffer_1),
                2,
                "\r\n", 2, "\\r\\n", 4,
                "\0", 1, "\\0", 2);
        string_replace(
                buffer_send + total_send_length - (64 > total_send_length ? total_send_length : 64), 64 > total_send_length ? total_send_length : 64,
                temp_buffer_2, sizeof(temp_buffer_2),
                2,
                "\r\n", 2, "\\r\\n", 4,
                "\0", 1, "\\0", 2);

        fprintf(
                stderr,
                "[ BUFFER SEND(%ld) ]\n'%s' (%lu)\n'%s' (%lu)\n\n",
                total_send_length,
                temp_buffer_1, strlen(temp_buffer_1),
                temp_buffer_2, strlen(temp_buffer_2));

        memset(temp_buffer_1, 0, sizeof(temp_buffer_1));
        memset(temp_buffer_2, 0, sizeof(temp_buffer_2));
        string_replace(
                buffer_recv, total_recv_length > 64 ? 64 : total_recv_length,
                temp_buffer_1, sizeof(temp_buffer_1),
                2,
                "\r\n", 2, "\\r\\n", 4,
                "\0", 1, "\\0", 2);
        string_replace(
                buffer_recv + total_recv_length - (64 > total_recv_length ? total_recv_length : 64), 64 > total_recv_length ? total_recv_length : 64,
                temp_buffer_2, sizeof(temp_buffer_2),
                2,
                "\r\n", 2, "\\r\\n", 4,
                "\0", 1, "\\0", 2);
        fprintf(
                stderr,
                "[ BUFFER RECV(%ld) ]\n'%s' (%lu)\n'%s' (%lu)\n\n",
                total_recv_length,
                temp_buffer_1, strlen(temp_buffer_1),
                temp_buffer_2, strlen(temp_buffer_2));

        memset(temp_buffer_1, 0, sizeof(temp_buffer_1));
        memset(temp_buffer_2, 0, sizeof(temp_buffer_2));
        string_replace(
                expected, expected_length > 64 ? 64 : expected_length,
                temp_buffer_1, sizeof(temp_buffer_1),
                2,
                "\r\n", 2, "\\r\\n", 4,
                "\0", 1, "\\0", 2);
        string_replace(
                expected + expected_length - (64 > expected_length ? expected_length : 64), 64 > expected_length ? expected_length : 64,
                temp_buffer_2, sizeof(temp_buffer_2),
                2,
                "\r\n", 2, "\\r\\n", 4,
                "\0", 1, "\\0", 2);
        fprintf(
                stderr,
                "[ EXPECTED RECV(%ld) ]\n'%s' (%lu)\n'%s' (%lu)\n\n",
                expected_length,
                temp_buffer_1, strlen(temp_buffer_1),
                temp_buffer_2, strlen(temp_buffer_2));
        fflush(stderr);
    }

    free(buffer_send);
    free(buffer_recv);

    return return_res;
}

bool send_recv_resp_command(
        int client_fd,
        const std::vector<std::string>& arguments,
        char *expected,
        size_t expected_length) {
    return send_recv_resp_command_multi_recv(client_fd, arguments, expected, expected_length, 1);
}

bool send_recv_resp_command_text(
        int client_fd,
        const std::vector<std::string>& arguments,
        char *expected) {
    return send_recv_resp_command(client_fd, arguments, expected, strlen(expected));
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
            .read_ms = -1,
            .write_ms = -1,
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

    int client_fd = network_io_common_socket_tcp4_new(0);
    address.sin_family = AF_INET;
    address.sin_port = htons(config_module_network_binding.port);
    address.sin_addr.s_addr = inet_addr(config_module_network_binding.host);

    REQUIRE(connect(client_fd, (struct sockaddr *) &address, sizeof(address)) == 0);

    SECTION("Redis - command - generic tests") {
        SECTION("Unknown / unsupported command") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"UNKNOWN COMMAND"},
                    "-ERR unknown command `UNKNOWN COMMAND` with `0` args\r\n"));
        }

        SECTION("Malformed - more data than declared") {
            snprintf(buffer_send, sizeof(buffer_send) - 1, "*1\r\n$5\r\nUNKNOWN COMMAND\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(client_fd, buffer_recv, sizeof(buffer_recv), 0) == 24);
            REQUIRE(strncmp(buffer_recv, "-ERR parsing error '8'\r\n",
                            strlen("-ERR parsing error '8'\r\n")) == 0);
        }

        SECTION("Timeout") {
            config_module_network_timeout.read_ms = 1000;

            // Send a NOP command to pick up the new timeout
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "a_value"},
                    "$-1\r\n"));

            // Wait the read timeout plus 100ms
            usleep((config.modules[0].network->timeout->read_ms * 1000) + (100 * 1000));

            // The socket should be closed so recv should return 0
            REQUIRE(recv(client_fd, buffer_recv, sizeof(buffer_recv), 0) == 0);
        }

        SECTION("Command too long") {
            int cmd_length = (int)config_module_redis.max_command_length + 1;
            char expected_error[256] = {0};
            char *allocated_buffer_send = (char*)malloc(cmd_length + 64);

            sprintf(
                    expected_error,
                    "-ERR the command length has exceeded '%u' bytes\r\n",
                    (int)config_module_redis.max_command_length);
            snprintf(
                    allocated_buffer_send,
                    cmd_length + 64,
                    "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$%d\r\n%0*d\r\n",
                    cmd_length,
                    cmd_length,
                    0);
            buffer_send_data_len = strlen(allocated_buffer_send);

            REQUIRE(send(client_fd, allocated_buffer_send, strlen(allocated_buffer_send), 0) == buffer_send_data_len);
            REQUIRE(recv(client_fd, buffer_recv, sizeof(buffer_recv), 0) == strlen(expected_error));
            REQUIRE(strncmp(buffer_recv, expected_error, strlen(expected_error)) == 0);

            free(allocated_buffer_send);
        }

        SECTION("Key too long - Positional") {
            int key_length = (int)config.modules[0].redis->max_key_length + 1;
            char expected_error[256] = { 0 };

            sprintf(
                    expected_error,
                    "-ERR The %s length has exceeded the allowed size of '%u'\r\n",
                    "key",
                    (int)config.modules[0].redis->max_key_length);
            snprintf(
                    buffer_send,
                    sizeof(buffer_send) - 1,
                    "*2\r\n$3\r\nGET\r\n$%d\r\n%0*d\r\n",
                    key_length,
                    key_length,
                    0);
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(client_fd, buffer_recv, sizeof(buffer_recv), 0) == strlen(expected_error));
            REQUIRE(strncmp(buffer_recv, expected_error, strlen(expected_error)) == 0);
        }

        SECTION("Key too long - Token") {
            int key_length = (int)config.modules[0].redis->max_key_length + 1;
            char expected_error[256] = { 0 };

            sprintf(
                    expected_error,
                    "-ERR The %s length has exceeded the allowed size of '%u'\r\n",
                    "key",
                    (int)config.modules[0].redis->max_key_length);
            snprintf(
                    buffer_send,
                    sizeof(buffer_send) - 1,
                    "*4\r\n$4\r\nSORT\r\n$1\r\na\r\n$5\r\nSTORE\r\n$%d\r\n%0*d\r\n",
                    key_length,
                    key_length,
                    0);
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(client_fd, buffer_recv, sizeof(buffer_recv), 0) == strlen(expected_error));
            REQUIRE(strncmp(buffer_recv, expected_error, strlen(expected_error)) == 0);
        }

        SECTION("Max command arguments") {
            off_t buffer_send_offset = 0;
            char expected_error[256] = { 0 };
            int arguments_count = (int)config.modules->redis->max_command_arguments + 1;

            buffer_send_offset += snprintf(
                    buffer_send + buffer_send_offset,
                    sizeof(buffer_send) - buffer_send_offset - 1,
                    "*%d\r\n$4\r\nMGET\r\n",
                    arguments_count + 1);

            for(int argument_index = 0; argument_index < arguments_count; argument_index++) {
                buffer_send_offset += snprintf(
                        buffer_send + buffer_send_offset,
                        sizeof(buffer_send) - buffer_send_offset - 1,
                        "$11\r\na_key_%05d\r\n",
                        argument_index);
            }

            buffer_send_data_len = strlen(buffer_send);

            sprintf(
                    expected_error,
                    "-ERR command '%s' has '%u' arguments but only '%u' allowed\r\n",
                    "MGET",
                    arguments_count,
                    (int)config.modules->redis->max_command_arguments);

            REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(client_fd, buffer_recv, sizeof(buffer_recv), 0) == strlen(expected_error));
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

            REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            size_t len = recv(client_fd, buffer_recv, sizeof(buffer_recv), 0);

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

            REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            size_t len = recv(client_fd, buffer_recv, sizeof(buffer_recv), 0);

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

            REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            size_t len = recv(client_fd, buffer_recv, sizeof(buffer_recv), 0);

            REQUIRE(len > strlen(hello_v3_expected_response_start) + strlen(hello_v3_expected_response_end));

            REQUIRE(strncmp(buffer_recv, hello_v3_expected_response_start, strlen(hello_v3_expected_response_start)) == 0);
            REQUIRE(strncmp(
                    buffer_recv + (len - strlen(hello_v3_expected_response_end)),
                    hello_v3_expected_response_end,
                    strlen(hello_v3_expected_response_end)) == 0);
        }
    }

    SECTION("Redis - command - SET") {
        SECTION("New key - short") {
            char *key = "a_key";
            char *value = "b_value";
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", key, value},
                    "+OK\r\n"));

            storage_db_entry_index_t *entry_index = storage_db_get_entry_index(db, key, strlen(key));
            REQUIRE(entry_index->value->sequence[0].chunk_length == strlen(value));
            REQUIRE(strncmp((char *) entry_index->value->sequence[0].memory.chunk_data, value, strlen(value)) == 0);
        }

        SECTION("New key - long") {
            char *key = "a_key";
            char *value = "this is a long key that can't be inlined";

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", key, value},
                    "+OK\r\n"));

            storage_db_entry_index_t *entry_index = storage_db_get_entry_index(db, key, strlen(key));
            REQUIRE(entry_index->value->sequence[0].chunk_length == strlen(value));
            REQUIRE(strncmp((char *) entry_index->value->sequence[0].memory.chunk_data, value, strlen(value)) == 0);
        }

        SECTION("Overwrite key") {
            char *key = "a_key";
            char *value1 = "b_value";
            char *value2 = "value_z";

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", key, value1},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", key, value2},
                    "+OK\r\n"));

            storage_db_entry_index_t *entry_index = storage_db_get_entry_index(db, key, strlen(key));
            REQUIRE(entry_index->value->sequence[0].chunk_length == strlen(value2));
            REQUIRE(strncmp((char *) entry_index->value->sequence[0].memory.chunk_data, value2, strlen(value2)) == 0);
        }

        SECTION("Missing parameters - key and value") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET"},
                    "-ERR wrong number of arguments for 'SET' command\r\n"));
        }

        SECTION("Missing parameters - value") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key"},
                    "-ERR wrong number of arguments for 'SET' command\r\n"));
        }

        SECTION("Too many parameters - one extra parameter") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "extra parameter"},
                    "-ERR syntax error\r\n"));
        }

        SECTION("New key - expire in 500ms") {
            char *key = "a_key";
            char *value = "b_value";
            config_module_network_timeout.read_ms = 1000;

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", key, value, "PX", "500"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", key},
                    "$7\r\nb_value\r\n"));

            // Wait for 600 ms and try to get the value after the expiration
            usleep((500 + 100) * 1000);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", key},
                    "$-1\r\n"));

            storage_db_entry_index_t *entry_index = storage_db_get_entry_index(db, key, strlen(key));
            REQUIRE(entry_index == NULL);
        }

        SECTION("New key - expire in 1s") {
            char *key = "a_key";
            char *value = "b_value";
            config_module_network_timeout.read_ms = 2000;

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", key, value, "EX", "1"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", key},
                    "$7\r\nb_value\r\n"));

            // Wait for 1100 ms and try to get the value after the expiration
            usleep((1000 + 100) * 1000);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", key},
                    "$-1\r\n"));

            storage_db_entry_index_t *entry_index = storage_db_get_entry_index(db, key, strlen(key));
            REQUIRE(entry_index == NULL);
        }

        SECTION("New key - KEEPTTL") {
            char *key = "a_key";
            char *value1 = "b_value";
            char *value2 = "value_z";
            config_module_network_timeout.read_ms = 1000;

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", key, value1, "PX", "500"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", key},
                    "$7\r\nb_value\r\n"));

            storage_db_entry_index_t *entry_index = storage_db_get_entry_index(db, key, strlen(key));
            REQUIRE(entry_index->value->sequence[0].chunk_length == strlen(value1));
            REQUIRE(strncmp((char *) entry_index->value->sequence[0].memory.chunk_data, value1, strlen(value1)) == 0);

            // Wait for 250 ms and then try to get the value and try to update the value keeping the same ttl
            // as the initial was in 500ms will expire after 250
            usleep(250 * 1000);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", key},
                    "$7\r\nb_value\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", key, value2, "KEEPTTL"},
                    "+OK\r\n"));

            entry_index = storage_db_get_entry_index(db, key, strlen(key));
            REQUIRE(entry_index->value->sequence[0].chunk_length == strlen(value2));
            REQUIRE(strncmp((char *) entry_index->value->sequence[0].memory.chunk_data, value2, strlen(value2)) == 0);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", key},
                    "$7\r\nvalue_z\r\n"));

            // Wait for 350 ms and try to get the value after the expiration
            usleep((250 + 100) * 1000);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", key},
                    "$-1\r\n"));

            entry_index = storage_db_get_entry_index(db, key, strlen(key));
            REQUIRE(entry_index == NULL);
        }

        SECTION("New key - XX") {
            SECTION("Key not existing") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "b_value", "XX"},
                        "$-1\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GET", "a_key"},
                        "$-1\r\n"));
            }

            SECTION("Key existing") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "b_value"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "c_value", "XX"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GET", "a_key"},
                        "$7\r\nc_value\r\n"));
            }

            SECTION("Key expired") {
                config_module_network_timeout.read_ms = 100000;

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "b_value", "PX", "500"},
                        "+OK\r\n"));

                // Wait for 600 ms and try to get the value after the expiration
                usleep((500 + 100) * 1000);

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "c_value", "XX"},
                        "$-1\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GET", "a_key"},
                        "$-1\r\n"));
            }
        }

        SECTION("New key - NX") {
            SECTION("Key not existing") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "b_value", "NX"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GET", "a_key"},
                        "$7\r\nb_value\r\n"));
            }

            SECTION("Key existing") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "b_value"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "c_value", "NX"},
                        "$-1\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GET", "a_key"},
                        "$7\r\nb_value\r\n"));
            }

            SECTION("Key expired") {
                config_module_network_timeout.read_ms = 1000;

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "b_value", "PX", "500"},
                        "+OK\r\n"));

                // Wait for 600 ms and try to get the value after the expiration
                usleep((500 + 100) * 1000);

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "c_value", "NX"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GET", "a_key"},
                        "$7\r\nc_value\r\n"));
            }
        }

        SECTION("New key - GET") {
            SECTION("Key not existing") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "b_value", "GET"},
                        "$-1\r\n"));
            }

            SECTION("Key existing") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "b_value"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "c_value", "GET"},
                        "$7\r\nb_value\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GET", "a_key"},
                        "$7\r\nc_value\r\n"));
            }

            SECTION("Key expired") {
                config_module_network_timeout.read_ms = 1000;

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "b_value", "PX", "500"},
                        "+OK\r\n"));

                // Wait for 600 ms and try to get the value after the expiration
                usleep((500 + 100) * 1000);

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "c_value", "GET"},
                        "$-1\r\n"));
            }

            SECTION("Multiple SET") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "b_value"},
                        "+OK\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "c_value", "GET"},
                        "$7\r\nb_value\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "d_value", "GET"},
                        "$7\r\nc_value\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"SET", "a_key", "e_value", "GET"},
                        "$7\r\nd_value\r\n"));

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GET", "a_key"},
                        "$7\r\ne_value\r\n"));
            }
        }

        SECTION("New key - SET with GET after key expired (test risk of deadlocks)") {
            config_module_network_timeout.read_ms = 2000;

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "PX", "500"},
                    "+OK\r\n"));

            // Wait for 600 ms and try to set the value after the expiration requesting to get returned the previous one
            usleep((500 + 100) * 1000);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "GET"},
                    "$-1\r\n"));
        }

        SECTION("New key - 4MB") {
            size_t long_value_length = 4 * 1024 * 1024;
            config_module_redis.max_command_length = long_value_length + 1024;

            // The long value is, on purpose, not filled with anything to have a very simple fuzzy testing (although
            // it's not repeatable)
            char *long_value = (char *) malloc(long_value_length + 1);

            // Fill with random data the long value
            char range = 'z' - 'a';
            for (size_t i = 0; i < long_value_length; i++) {
                long_value[i] = (char) (i % range) + 'a';
            }

            // This is legit as long_value_length + 1 is actually being allocated
            long_value[long_value_length] = 0;

            size_t expected_response_length = snprintf(
                    nullptr,
                    0,
                    "$%lu\r\n%.*s\r\n",
                    long_value_length,
                    (int) long_value_length,
                    long_value);

            char *expected_response = (char *) malloc(expected_response_length + 1);
            snprintf(
                    expected_response,
                    expected_response_length + 1,
                    "$%lu\r\n%.*s\r\n",
                    long_value_length,
                    (int) long_value_length,
                    long_value);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", long_value},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_multi_recv(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    expected_response,
                    expected_response_length,
                    send_recv_resp_command_calculate_multi_recv(long_value_length)));

            free(expected_response);
        }

        SECTION("New key - 256MB") {
            size_t long_value_length = 256 * 1024 * 1024;
            config_module_redis.max_command_length = long_value_length + 1024;

            // The long value is, on purpose, not filled with anything to have a very simple fuzzy testing (although
            // it's not repeatable)
            char *long_value = (char *) malloc(long_value_length + 1);

            // Fill with random data the long value
            char range = 'z' - 'a';
            for (size_t i = 0; i < long_value_length; i++) {
                long_value[i] = (char) (i % range) + 'a';
            }

            // This is legit as long_value_length + 1 is actually being allocated
            long_value[long_value_length] = 0;

            size_t expected_response_length = snprintf(
                    nullptr,
                    0,
                    "$%lu\r\n%.*s\r\n",
                    long_value_length,
                    (int) long_value_length,
                    long_value);

            char *expected_response = (char *) malloc(expected_response_length + 1);
            snprintf(
                    expected_response,
                    expected_response_length + 1,
                    "$%lu\r\n%.*s\r\n",
                    long_value_length,
                    (int) long_value_length,
                    long_value);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", long_value},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_multi_recv(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    expected_response,
                    expected_response_length,
                    send_recv_resp_command_calculate_multi_recv(long_value_length)));

            free(expected_response);
        }
    }

    SECTION("Redis - command - GET") {
        SECTION("Existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    "$7\r\nb_value\r\n"));
        }

        SECTION("Existing key - pipelining") {
            char buffer_recv_expected[512] = { 0 };
            char *buffer_send_start = buffer_send;
            char *buffer_recv_expected_start = buffer_recv_expected;

            snprintf(buffer_send, sizeof(buffer_send) - 1, "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$7\r\nb_value\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(client_fd, buffer_recv, sizeof(buffer_recv), 0) == 5);
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

            REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);

            size_t recv_len = 0;
            do {
                recv_len += recv(client_fd, buffer_recv, sizeof(buffer_recv), 0);
            } while(recv_len < 130);

            REQUIRE(strncmp(buffer_recv, buffer_recv_expected_start, strlen(buffer_recv_expected_start)) == 0);
        }

        SECTION("Non-existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    "$-1\r\n"));
        }

        SECTION("Existent key - 4MB") {
            size_t long_value_length = 4 * 1024 * 1024;
            config_module_redis.max_command_length = long_value_length + 1024;

            // The long value is, on purpose, not filled with anything to have a very simple fuzzy testing (although
            // it's not repeatable)
            char *long_value = (char *) malloc(long_value_length + 1);

            // Fill with random data the long value
            char range = 'z' - 'a';
            for (size_t i = 0; i < long_value_length; i++) {
                long_value[i] = (char) (i % range) + 'a';
            }

            // This is legit as long_value_length + 1 is actually being allocated
            long_value[long_value_length] = 0;

            size_t expected_response_length = snprintf(
                    nullptr,
                    0,
                    "$%lu\r\n%.*s\r\n",
                    long_value_length,
                    (int) long_value_length,
                    long_value);

            char *expected_response = (char *) malloc(expected_response_length + 1);
            snprintf(
                    expected_response,
                    expected_response_length + 1,
                    "$%lu\r\n%.*s\r\n",
                    long_value_length,
                    (int) long_value_length,
                    long_value);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", long_value},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_multi_recv(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    expected_response,
                    expected_response_length,
                    send_recv_resp_command_calculate_multi_recv(long_value_length)));

            free(expected_response);
        }

        SECTION("Missing parameters - key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET"},
                    "-ERR wrong number of arguments for 'GET' command\r\n"));
        }
    }

    SECTION("Redis - command - GETDEL") {
        SECTION("Existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETDEL", "a_key"},
                    "$7\r\nb_value\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    "$-1\r\n"));
        }

        SECTION("Non-existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETDEL", "a_key"},
                    "$-1\r\n"));
        }
    }

    SECTION("Redis - command - GETRANGE") {
        SECTION("Non-existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    "$-1\r\n"));
        }

        SECTION("Existing key - short value") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            SECTION("all") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", "0", "6"},
                        "$7\r\nb_value\r\n"));
            }

            SECTION("first char") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", "0", "0"},
                        "$1\r\nb\r\n"));
            }

            SECTION("last char") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", "6", "6"},
                        "$1\r\ne\r\n"));
            }

            SECTION("last char - negative start negative end") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", "-1", "-1"},
                        "$1\r\ne\r\n"));
            }

            SECTION("from the last fourth char to the second from the last") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", "-4", "-2"},
                        "$3\r\nalu\r\n"));
            }

            SECTION("end before start") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", "5", "3"},
                        "$0\r\n\r\n"));
            }

            SECTION("start after end") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", "7", "15"},
                        "$0\r\n\r\n"));
            }

            SECTION("start before end, length after end") {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", "3", "15"},
                        "$4\r\nalue\r\n"));
            }
        }

        SECTION("Existing key - long value") {
            char *expected_response = NULL;
            char *expected_response_static[256] = { 0 };
            size_t long_value_length = 4 * 1024 * 1024;
            config_module_redis.max_command_length = long_value_length + 1024;

            // The long value is, on purpose, not filled with anything to have a very simple fuzzy testing (although
            // it's not repeatable)
            char *long_value = (char *) malloc(long_value_length + 1);

            // Fill with random data the long value
            char range = 'z' - 'a';
            for (size_t i = 0; i < long_value_length; i++) {
                long_value[i] = (char) (i % range) + 'a';
            }

            // This is legit as long_value_length + 1 is actually being allocated
            long_value[long_value_length] = 0;

            char end[32] = { 0 };
            snprintf(end, sizeof(end), "%lu", long_value_length - 1);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", long_value},
                    "+OK\r\n"));

            SECTION("all") {
                size_t expected_response_length = snprintf(
                        nullptr,
                        0,
                        "$%lu\r\n%.*s\r\n",
                        long_value_length,
                        (int) long_value_length,
                        long_value);

                expected_response = (char *) malloc(expected_response_length + 1);
                snprintf(
                        expected_response,
                        expected_response_length + 1,
                        "$%lu\r\n%.*s\r\n",
                        long_value_length,
                        (int) long_value_length,
                        long_value);

                REQUIRE(send_recv_resp_command_multi_recv(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", "0", end},
                        expected_response,
                        expected_response_length,
                        send_recv_resp_command_calculate_multi_recv(long_value_length)));
            }

            SECTION("first char") {
                snprintf((char*)expected_response_static, sizeof(expected_response_static), "$1\r\n%c\r\n", long_value[0]);

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", "0", "0"},
                        (char*)expected_response_static));
            }

            SECTION("last char") {
                snprintf(
                        (char*)expected_response_static,
                        sizeof(expected_response_static),
                        "$1\r\n%c\r\n",
                        long_value[long_value_length - 1]);

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", end, end},
                        (char*)expected_response_static));
            }

            SECTION("last char - negative start negative end") {
                snprintf(
                        (char*)expected_response_static,
                        sizeof(expected_response_static),
                        "$1\r\n%c\r\n",
                        long_value[long_value_length - 1]);

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", "-1", "-1"},
                        (char*)expected_response_static));
            }

            SECTION("from the last fourth char to the second from the last") {
                snprintf(
                        (char*)expected_response_static,
                        sizeof(expected_response_static),
                        "$3\r\n%.*s\r\n",
                        3,
                        long_value + long_value_length - 4);

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", "-4", "-2"},
                        (char*)expected_response_static));
            }

            SECTION("end before start") {
                char start1[32] = { 0 };
                snprintf(start1, sizeof(start1), "%lu", long_value_length - 50);
                char end1[32] = { 0 };
                snprintf(end1, sizeof(end1), "%lu", long_value_length - 100);

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", start1, end1},
                        "$0\r\n\r\n"));
            }

            SECTION("start after end") {
                char start1[32] = { 0 };
                snprintf(start1, sizeof(start1), "%lu", long_value_length + 50);
                char end1[32] = { 0 };
                snprintf(end1, sizeof(end1), "%lu", long_value_length + 100);

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", start1, end1},
                        "$0\r\n\r\n"));
            }

            SECTION("start before end, length after end") {
                char start1[32] = { 0 };
                snprintf(start1, sizeof(start1), "%lu", long_value_length -4);
                char end1[32] = { 0 };
                snprintf(end1, sizeof(end1), "%lu", long_value_length + 100);

                snprintf(
                        (char*)expected_response_static,
                        sizeof(expected_response_static),
                        "$4\r\n%s\r\n",
                        long_value + long_value_length - 4);

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", start1, end1},
                        (char*)expected_response_static));
            }

            SECTION("50 chars after first chunk + 50") {
                off_t start1_int = ((64 * 1024) - 1) + 50;
                off_t end1_int = start1_int + 50;
                char start1[32] = { 0 };
                snprintf(start1, sizeof(start1), "%lu", start1_int);
                char end1[32] = { 0 };
                snprintf(end1, sizeof(end1), "%lu", end1_int);

                size_t expected_response_length = snprintf(
                        nullptr,
                        0,
                        "$%lu\r\n%.*s\r\n",
                        end1_int - start1_int + 1,
                        (int)(end1_int - start1_int + 1),
                        long_value + start1_int);

                expected_response = (char *)malloc(expected_response_length + 1);
                snprintf(
                        expected_response,
                        expected_response_length + 1,
                        "$%lu\r\n%.*s\r\n",
                        end1_int - start1_int + 1,
                        (int)(end1_int - start1_int + 1),
                        long_value + start1_int);

                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{"GETRANGE", "a_key", start1, end1},
                        (char*)expected_response));
            }

            if (expected_response) {
                free(expected_response);
            }
        }

        SECTION("Missing parameters - key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET"},
                    "-ERR wrong number of arguments for 'GET' command\r\n"));
        }
    }

    SECTION("Redis - command - GETEX") {
        SECTION("Existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETEX", "a_key"},
                    "$7\r\nb_value\r\n"));
        }

        SECTION("Non-existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETEX", "a_key"},
                    "$-1\r\n"));
        }

        SECTION("New key - expire in 500ms") {
            config_module_network_timeout.read_ms = 1000;

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETEX", "a_key", "PX", "500"},
                    "$7\r\nb_value\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    "$7\r\nb_value\r\n"));

            // Wait for 600 ms and try to get the value after the expiration
            usleep((500 + 100) * 1000);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    "$-1\r\n"));

            storage_db_entry_index_t *entry_index = storage_db_get_entry_index(db, "a_key", strlen("a_key"));
            REQUIRE(entry_index == NULL);
        }

        SECTION("New key - expire in 1s") {
            config_module_network_timeout.read_ms = 2000;

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETEX", "a_key", "EX", "1"},
                    "$7\r\nb_value\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    "$7\r\nb_value\r\n"));

            // Wait for 1100 ms and try to get the value after the expiration
            usleep((1000 + 100) * 1000);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    "$-1\r\n"));

            storage_db_entry_index_t *entry_index = storage_db_get_entry_index(db, "a_key", strlen("a_key"));
            REQUIRE(entry_index == NULL);
        }
    }

    SECTION("Redis - command - GETSET") {
        SECTION("Non-existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETSET", "a_key", "b_value"},
                    "$-1\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    "$7\r\nb_value\r\n"));
        }

        SECTION("Existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GETSET", "a_key", "z_value"},
                    "$7\r\nb_value\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    "$7\r\nz_value\r\n"));
        }
    }

    SECTION("Redis - command - COPY") {
        SECTION("Non existant key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"COPY", "a_key", "b_key"},
                    ":0\r\n"));
        }

        SECTION("Existent key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"COPY", "a_key", "b_key"},
                    ":1\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "b_key"},
                    "$7\r\nb_value\r\n"));
        }

        SECTION("Existent key - 4MB") {
            size_t long_value_length = 4 * 1024 * 1024;
            config_module_redis.max_command_length = long_value_length + 1024;

            // The long value is, on purpose, not filled with anything to have a very simple fuzzy testing (although
            // it's not repeatable)
            char *long_value = (char *) malloc(long_value_length + 1);

            // Fill with random data the long value
            char range = 'z' - 'a';
            for (size_t i = 0; i < long_value_length; i++) {
                long_value[i] = (char) (i % range) + 'a';
            }

            // This is legit as long_value_length + 1 is actually being allocated
            long_value[long_value_length] = 0;

            size_t expected_response_length = snprintf(
                    nullptr,
                    0,
                    "$%lu\r\n%.*s\r\n",
                    long_value_length,
                    (int) long_value_length,
                    long_value);

            char *expected_response = (char *) malloc(expected_response_length + 1);
            snprintf(
                    expected_response,
                    expected_response_length + 1,
                    "$%lu\r\n%.*s\r\n",
                    long_value_length,
                    (int) long_value_length,
                    long_value);

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", long_value},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"COPY", "a_key", "b_key"},
                    ":1\r\n"));

            REQUIRE(send_recv_resp_command_multi_recv(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    expected_response,
                    expected_response_length,
                    send_recv_resp_command_calculate_multi_recv(long_value_length)));

            REQUIRE(send_recv_resp_command_multi_recv(
                    client_fd,
                    std::vector<std::string>{"GET", "b_key"},
                    expected_response,
                    expected_response_length,
                    send_recv_resp_command_calculate_multi_recv(long_value_length)));

            free(expected_response);
        }

        SECTION("Existent destination key - fail") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key", "b_value", "b_key", "value_z"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"COPY", "a_key", "b_key"},
                    ":0\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "b_key"},
                    "$7\r\nvalue_z\r\n"));
        }

        SECTION("Existent destination key - Overwrite") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key", "b_value", "b_key", "value_z"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"COPY", "a_key", "b_key", "REPLACE"},
                    ":1\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "b_key"},
                    "$7\r\nb_value\r\n"));
        }
    }

    SECTION("Redis - command - DEL") {
        SECTION("Existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"DEL", "a_key"},
                    ":1\r\n"));
        }

        SECTION("Multiple key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key", "b_value", "b_key", "value_z", "c_key", "d_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"DEL", "a_key", "b_key", "c_key"},
                    ":3\r\n"));
        }

        SECTION("Non-existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"DEL", "a_key"},
                    ":0\r\n"));
        }

        SECTION("Missing parameters - key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"DEL"},
                    "-ERR wrong number of arguments for 'DEL' command\r\n"));
        }
    }

    SECTION("Redis - command - UNLINK") {
        SECTION("Existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"UNLINK", "a_key"},
                    ":1\r\n"));
        }

        SECTION("Multiple key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key", "b_value", "b_key", "value_z", "c_key", "d_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"UNLINK", "a_key", "b_key", "c_key"},
                    ":3\r\n"));
        }

        SECTION("Non-existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"UNLINK", "a_key"},
                    ":0\r\n"));
        }

        SECTION("Missing parameters - key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"UNLINK"},
                    "-ERR wrong number of arguments for 'UNLINK' command\r\n"));
        }
    }

    SECTION("Redis - command - TOUCH") {
        SECTION("Existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"TOUCH", "a_key"},
                    ":1\r\n"));
        }

        SECTION("Multiple key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key", "b_value", "b_key", "value_z", "c_key", "d_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"TOUCH", "a_key", "b_key", "c_key"},
                    ":3\r\n"));
        }

        SECTION("Non-existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"TOUCH", "a_key"},
                    ":0\r\n"));
        }
    }

    SECTION("Redis - command - MGET") {
        SECTION("Existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MGET", "a_key"},
                    "*1\r\n$7\r\nb_value\r\n"));
        }

        SECTION("Existing key - pipelining") {
            char buffer_recv_expected[512] = { 0 };
            char *buffer_send_start = buffer_send;
            char *buffer_recv_expected_start = buffer_recv_expected;

            snprintf(buffer_send, sizeof(buffer_send) - 1, "*3\r\n$3\r\nSET\r\n$5\r\na_key\r\n$7\r\nb_value\r\n");
            buffer_send_data_len = strlen(buffer_send);

            REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE(recv(client_fd, buffer_recv, sizeof(buffer_recv), 0) == 5);
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

            REQUIRE(send(client_fd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);

            size_t recv_len = 0;
            do {
                recv_len += recv(client_fd, buffer_recv, sizeof(buffer_recv), 0);
            } while(recv_len < 130);

            REQUIRE(strncmp(buffer_recv, buffer_recv_expected_start, strlen(buffer_recv_expected_start)) == 0);
        }

        SECTION("Non-existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MGET", "a_key"},
                    "*1\r\n$-1\r\n"));
        }

        SECTION("Missing parameters - key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MGET"},
                    "-ERR wrong number of arguments for 'MGET' command\r\n"));
        }

        SECTION("Fetch 2 keys") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key_1", "b_value_1"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key_2", "b_value_2"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MGET", "a_key_1", "a_key_2"},
                    "*2\r\n$9\r\nb_value_1\r\n$9\r\nb_value_2\r\n"));
        }

        SECTION("Fetch 128 keys") {
            int key_count = 128;
            std::vector<std::string> arguments = std::vector<std::string>();
            char buffer_recv_cmp[8 * 1024] = { 0 };
            size_t buffer_recv_cmp_length;
            off_t buffer_recv_cmp_offset = 0;
            off_t buffer_send_offset = 0;

            for(int key_index = 0; key_index < key_count; key_index++) {
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{
                                "SET",
                                string_format("a_key_%05d", key_index),
                                string_format("b_value_%05d", key_index)
                        },
                        "+OK\r\n"));
            }

            buffer_recv_cmp_offset += snprintf(
                    buffer_recv_cmp + buffer_recv_cmp_offset,
                    sizeof(buffer_recv_cmp) - buffer_recv_cmp_offset - 1,
                    "*128\r\n");

            arguments.emplace_back("MGET");
            for(int key_index = 0; key_index < key_count; key_index++) {
                arguments.push_back(string_format("a_key_%05d", key_index));

                buffer_recv_cmp_offset += snprintf(
                        buffer_recv_cmp + buffer_recv_cmp_offset,
                        sizeof(buffer_recv_cmp) - buffer_recv_cmp_offset - 1,
                        "$13\r\nb_value_%05d\r\n",
                        key_index);
            }

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    arguments,
                    buffer_recv_cmp));
        }
    }

    SECTION("Redis - command - MSET") {
        SECTION("1 key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    "$7\r\nb_value\r\n"));
        }

        SECTION("2 keys") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key", "b_value", "b_key", "value_z"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    "$7\r\nb_value\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "b_key"},
                    "$7\r\nvalue_z\r\n"));
        }

        SECTION("Set 64 keys") {
            int key_count = 64;
            std::vector<std::string> arguments = std::vector<std::string>();

            arguments.emplace_back("MSET");
            for(int key_index = 0; key_index < key_count; key_index++) {
                arguments.push_back(string_format("a_key_%05d", key_index));
                arguments.push_back(string_format("b_value_%05d", key_index));
            }

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    arguments,
                    "+OK\r\n"));

            for(int key_index = 0; key_index < key_count; key_index++) {
                char expected_response[32] = { 0 };
                REQUIRE(send_recv_resp_command_text(
                        client_fd,
                        std::vector<std::string>{ "GET", string_format("a_key_%05d", key_index) },
                        (char*)(string_format("$13\r\nb_value_%05d\r\n", key_index).c_str())));
            }
        }

        SECTION("Missing parameters - key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET"},
                    "-ERR wrong number of arguments for 'MSET' command\r\n"));
        }

        SECTION("Missing parameters - value") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key"},
                    "-ERR wrong number of arguments for 'MSET' command\r\n"));
        }
    }

    SECTION("Redis - command - MSETNX") {
        SECTION("1 key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSETNX", "a_key", "b_value"},
                    ":1\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    "$7\r\nb_value\r\n"));
        }

        SECTION("2 keys") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSETNX", "a_key", "b_value", "b_key", "value_z"},
                    ":1\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    "$7\r\nb_value\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "b_key"},
                    "$7\r\nvalue_z\r\n"));
        }

        SECTION("Existing keys") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSETNX", "a_key", "b_value", "b_key", "value_z"},
                    ":1\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "a_key"},
                    "$7\r\nb_value\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "b_key"},
                    "$7\r\nvalue_z\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSETNX", "b_key", "another_value", "c_key", "value_not_being_set"},
                    ":0\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "b_key"},
                    "$7\r\nvalue_z\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"GET", "c_key"},
                    "$-1\r\n"));
        }

        // TODO: need to fix rmw multi key transactions first
//        SECTION("Set 64 keys") {
//            int key_count = 64;
//            std::vector<std::string> arguments = std::vector<std::string>();
//
//            arguments.emplace_back("MSETNX");
//            for(int key_index = 0; key_index < key_count; key_index++) {
//                arguments.push_back(string_format("a_key_%05d", key_index));
//                arguments.push_back(string_format("b_value_%05d", key_index));
//            }
//
//            REQUIRE(send_recv_resp_command_text(
//                    client_fd,
//                    arguments,
//                    ":1\r\n"));
//
//            for(int key_index = 0; key_index < key_count; key_index++) {
//                char expected_response[32] = { 0 };
//                REQUIRE(send_recv_resp_command_text(
//                        client_fd,
//                        std::vector<std::string>{ "GET", string_format("a_key_%05d", key_index) },
//                        (char*)(string_format("$13\r\nb_value_%05d\r\n", key_index).c_str())));
//            }
//        }

        SECTION("Missing parameters - key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSETNX"},
                    "-ERR wrong number of arguments for 'MSETNX' command\r\n"));
        }

        SECTION("Missing parameters - value") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSETNX", "a_key"},
                    "-ERR wrong number of arguments for 'MSETNX' command\r\n"));
        }
    }

    SECTION("Redis - command - EXISTS") {
        SECTION("Non-existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXISTS", "a_key"},
                    ":0\r\n"));
        }

        SECTION("1 key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXISTS", "a_key"},
                    ":1\r\n"));
        }

        SECTION("2 keys") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key", "b_value", "b_key", "value_z"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXISTS", "a_key", "b_key"},
                    ":2\r\n"));
        }

        SECTION("Repeated key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXISTS", "a_key", "a_key", "a_key"},
                    ":3\r\n"));
        }
    }

    SECTION("Redis - command - TTL") {
        SECTION("Non-existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"TTL", "a_key"},
                    ":-2\r\n"));
        }

        SECTION("Key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"TTL", "a_key"},
                    ":-1\r\n"));
        }

        SECTION("Key with 5 second expire") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "5"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"TTL", "a_key"},
                    ":5\r\n"));
        }
    }

    SECTION("Redis - command - PTTL") {
        SECTION("Non-existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PTTL", "a_key"},
                    ":-2\r\n"));
        }

        SECTION("Key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PTTL", "a_key"},
                    ":-1\r\n"));
        }

        SECTION("Key with 5 second expire") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "5"},
                    "+OK\r\n"));

            // Potentially flaky test
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PTTL", "a_key"},
                    ":5000\r\n"));
        }
    }

    SECTION("Redis - command - PERSIST") {
        SECTION("No key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PERSIST", "a_key"},
                    ":0\r\n"));
        }

        SECTION("Key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PERSIST", "a_key"},
                    ":0\r\n"));
        }

        SECTION("Key with expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "5"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PERSIST", "a_key"},
                    ":1\r\n"));
        }
    }

    SECTION("Redis - command - EXPIRE") {
        SECTION("No key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIRE", "a_key", "5"},
                    ":0\r\n"));
        }

        SECTION("Existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIRE", "a_key", "5"},
                    ":1\r\n"));
        }

        SECTION("Existing key - NX - key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIRE", "a_key", "5", "NX"},
                    ":1\r\n"));
        }

        SECTION("Existing key - NX - key with expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "5"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIRE", "a_key", "5", "NX"},
                    ":0\r\n"));
        }

        SECTION("Existing key - XX - key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIRE", "a_key", "5", "XX"},
                    ":0\r\n"));
        }

        SECTION("Existing key - XX - key with expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "5"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIRE", "a_key", "5", "XX"},
                    ":1\r\n"));
        }

        SECTION("Existing key - GT - key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIRE", "a_key", "5", "GT"},
                    ":0\r\n"));
        }

        SECTION("Existing key - GT - key with expiry - lower than") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "7"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIRE", "a_key", "5", "GT"},
                    ":0\r\n"));
        }

        SECTION("Existing key - GT - key with expiry - greater than") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "3"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIRE", "a_key", "5", "GT"},
                    ":1\r\n"));
        }

        SECTION("Existing key - LT - key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIRE", "a_key", "5", "LT"},
                    ":1\r\n"));
        }

        SECTION("Existing key - LT - key with expiry - lower than") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "7"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIRE", "a_key", "5", "LT"},
                    ":1\r\n"));
        }

        SECTION("Existing key - LT - key with expiry - greater than") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "3"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIRE", "a_key", "5", "LT"},
                    ":0\r\n"));
        }
    }

    SECTION("Redis - command - EXPIREAT") {
        char unixtime_plus_5s_str[32] = { 0 };
        int64_t unixtime_plus_5 = (clock_realtime_coarse_int64_ms() / 1000) + 5;
        snprintf(unixtime_plus_5s_str, sizeof(unixtime_plus_5s_str), "%ld", unixtime_plus_5);

        SECTION("No key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIREAT", "a_key", unixtime_plus_5s_str},
                    ":0\r\n"));
        }

        SECTION("Existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIREAT", "a_key", unixtime_plus_5s_str},
                    ":1\r\n"));
        }

        SECTION("Existing key - NX - key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIREAT", "a_key", unixtime_plus_5s_str, "NX"},
                    ":1\r\n"));
        }

        SECTION("Existing key - NX - key with expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "5"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIREAT", "a_key", unixtime_plus_5s_str, "NX"},
                    ":0\r\n"));
        }

        SECTION("Existing key - XX - key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIREAT", "a_key", unixtime_plus_5s_str, "XX"},
                    ":0\r\n"));
        }

        SECTION("Existing key - XX - key with expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "5"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIREAT", "a_key", unixtime_plus_5s_str, "XX"},
                    ":1\r\n"));
        }

        SECTION("Existing key - GT - key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIREAT", "a_key", unixtime_plus_5s_str, "GT"},
                    ":0\r\n"));
        }

        SECTION("Existing key - GT - key with expiry - lower than") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "7"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIREAT", "a_key", unixtime_plus_5s_str, "GT"},
                    ":0\r\n"));
        }

        SECTION("Existing key - GT - key with expiry - greater than") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "3"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIREAT", "a_key", unixtime_plus_5s_str, "GT"},
                    ":1\r\n"));
        }

        SECTION("Existing key - LT - key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIREAT", "a_key", unixtime_plus_5s_str, "LT"},
                    ":1\r\n"));
        }

        SECTION("Existing key - LT - key with expiry - lower than") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "7"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIREAT", "a_key", unixtime_plus_5s_str, "LT"},
                    ":1\r\n"));
        }

        SECTION("Existing key - LT - key with expiry - greater than") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "3"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIREAT", "a_key", unixtime_plus_5s_str, "LT"},
                    ":0\r\n"));
        }
    }

    SECTION("Redis - command - PEXPIRE") {
        SECTION("No key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIRE", "a_key", "5000"},
                    ":0\r\n"));
        }

        SECTION("Existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIRE", "a_key", "5000"},
                    ":1\r\n"));
        }

        SECTION("Existing key - NX - key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIRE", "a_key", "5000", "NX"},
                    ":1\r\n"));
        }

        SECTION("Existing key - NX - key with expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "5"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIRE", "a_key", "5000", "NX"},
                    ":0\r\n"));
        }

        SECTION("Existing key - XX - key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIRE", "a_key", "5000", "XX"},
                    ":0\r\n"));
        }

        SECTION("Existing key - XX - key with expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "5"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIRE", "a_key", "5000", "XX"},
                    ":1\r\n"));
        }

        SECTION("Existing key - GT - key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIRE", "a_key", "5000", "GT"},
                    ":0\r\n"));
        }

        SECTION("Existing key - GT - key with expiry - lower than") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "7"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIRE", "a_key", "5000", "GT"},
                    ":0\r\n"));
        }

        SECTION("Existing key - GT - key with expiry - greater than") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "3"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIRE", "a_key", "5000", "GT"},
                    ":1\r\n"));
        }

        SECTION("Existing key - LT - key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIRE", "a_key", "5000", "LT"},
                    ":1\r\n"));
        }

        SECTION("Existing key - LT - key with expiry - lower than") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "7"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIRE", "a_key", "5000", "LT"},
                    ":1\r\n"));
        }

        SECTION("Existing key - LT - key with expiry - greater than") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "3"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIRE", "a_key", "5000", "LT"},
                    ":0\r\n"));
        }
    }

    SECTION("Redis - command - PEXPIREAT") {
        char unixtime_ms_plus_5s_str[32] = {0 };
        int64_t unixtime_ms_plus_5s = clock_realtime_coarse_int64_ms() + 5000;
        snprintf(unixtime_ms_plus_5s_str, sizeof(unixtime_ms_plus_5s_str), "%ld", unixtime_ms_plus_5s);

        SECTION("No key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str},
                    ":0\r\n"));
        }

        SECTION("Existing key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str},
                    ":1\r\n"));
        }

        SECTION("Existing key - NX - key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "NX"},
                    ":1\r\n"));
        }

        SECTION("Existing key - NX - key with expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "5"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "NX"},
                    ":0\r\n"));
        }

        SECTION("Existing key - XX - key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "XX"},
                    ":0\r\n"));
        }

        SECTION("Existing key - XX - key with expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "5"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "XX"},
                    ":1\r\n"));
        }

        SECTION("Existing key - GT - key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "GT"},
                    ":0\r\n"));
        }

        SECTION("Existing key - GT - key with expiry - lower than") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "7"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "GT"},
                    ":0\r\n"));
        }

        SECTION("Existing key - GT - key with expiry - greater than") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "3"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "GT"},
                    ":1\r\n"));
        }

        SECTION("Existing key - LT - key without expiry") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "LT"},
                    ":1\r\n"));
        }

        SECTION("Existing key - LT - key with expiry - lower than") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "7"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "LT"},
                    ":1\r\n"));
        }

        SECTION("Existing key - LT - key with expiry - greater than") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "3"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIREAT", "a_key", unixtime_ms_plus_5s_str, "LT"},
                    ":0\r\n"));
        }
    }

    SECTION("Redis - command - EXPIRETIME") {
        SECTION("No key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIRETIME", "a_key"},
                    ":-2\r\n"));
        }

        SECTION("Existing key - no expiration") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIRETIME", "a_key"},
                    ":-1\r\n"));
        }

        SECTION("Existing key - expiration") {
            int64_t unixtime_plus_5s = (clock_realtime_coarse_int64_ms() / 1000) + 5;

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "5"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"EXPIRETIME", "a_key"},
                    (char*)string_format(":%ld\r\n", unixtime_plus_5s).c_str()));
        }
    }

    SECTION("Redis - command - PEXPIRETIME") {
        SECTION("No key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIRETIME", "a_key"},
                    ":-2\r\n"));
        }

        SECTION("Existing key - no expiration") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIRETIME", "a_key"},
                    ":-1\r\n"));
        }

        SECTION("Existing key - expiration") {
            int64_t unixtime_ms_plus_5s = clock_realtime_coarse_int64_ms() + 5000;

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value", "EX", "5"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PEXPIRETIME", "a_key"},
                    (char*)string_format(":%ld\r\n", unixtime_ms_plus_5s).c_str()));
        }
    }

    SECTION("Redis - command - LCS") {
        SECTION("Missing keys - String") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"LCS", "a_key", "b_key"},
                    "$0\r\n\r\n"));
        }

        SECTION("Missing keys - Length") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"LCS", "a_key", "b_key", "LEN"},
                    ":0\r\n"));
        }

        SECTION("Empty keys - String") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key", "", "b_key", ""},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"LCS", "a_key", "b_key"},
                    "$0\r\n\r\n"));
        }

        SECTION("Empty keys - Length") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key", "", "b_key", ""},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"LCS", "a_key", "b_key", "LEN"},
                    ":0\r\n"));
        }

        SECTION("No common substrings - String") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key", "qwertyuiop", "b_key", "asdfghjkl"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"LCS", "a_key", "b_key"},
                    "$0\r\n\r\n"));
        }

        SECTION("No common substrings - Length") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key", "qwertyuiop", "b_key", "asdfghjkl"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"LCS", "a_key", "b_key", "LEN"},
                    ":0\r\n"));
        }

        SECTION("One common substring - String") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key", "b_value", "b_key", "value_z"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"LCS", "a_key", "b_key"},
                    "$5\r\nvalue\r\n"));
        }

        SECTION("One common substring - String") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"MSET", "a_key", "b_value", "b_key", "value_z"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"LCS", "a_key", "b_key", "LEN"},
                    ":5\r\n"));
        }

        SECTION("Multiple common substring - String") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{
                        "MSET",
                        "a_key",
                        "a very long string",
                        "b_key",
                        "another very string but long"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"LCS", "a_key", "b_key"},
                    "$13\r\na very string\r\n"));
        }

        SECTION("Multiple common substring - Length") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{
                            "MSET",
                            "a_key",
                            "a very long string",
                            "b_key",
                            "another very string but long"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"LCS", "a_key", "b_key", "LEN"},
                    ":13\r\n"));
        }
    }

    SECTION("Redis - command - DBSIZE") {
        SECTION("Empty database") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"DBSIZE"},
                    ":0\r\n"));
        }

        SECTION("Database with 1 key") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"DBSIZE"},
                    ":1\r\n"));
        }

        SECTION("Database with 1 key overwritten") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "value_z"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"DBSIZE"},
                    ":1\r\n"));
        }

        SECTION("Database with 1 key deleted") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"DEL", "a_key"},
                    ":1\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"DBSIZE"},
                    ":0\r\n"));
        }

        SECTION("Database with 1 key flushed") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"SET", "a_key", "b_value"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"FLUSHDB"},
                    "+OK\r\n"));

            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"DBSIZE"},
                    ":0\r\n"));
        }
    }

    SECTION("Redis - command - PING") {
        SECTION("Without value") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PING"},
                    "$4\r\nPONG\r\n"));
        }

        SECTION("With value") {
            REQUIRE(send_recv_resp_command_text(
                    client_fd,
                    std::vector<std::string>{"PING", "a test"},
                    "$6\r\na test\r\n"));
        }
    }

    SECTION("Redis - command - QUIT") {
        send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"QUIT"},
                "$2\r\nOK\r\n");
    }

    SECTION("Redis - command - SHUTDOWN") {
        send_recv_resp_command_text(
                client_fd,
                std::vector<std::string>{"SHUTDOWN"},
                "$2\r\nOK\r\n");

        // Wait 5 seconds in addition to the max duration of the wait time in the loop to ensure that the worker has
        // plenty of time to shut-down
        usleep((5000 + (WORKER_LOOP_MAX_WAIT_TIME_MS + 100)) * 1000);
        MEMORY_FENCE_LOAD();
        REQUIRE(!worker_context->running);
    }

    close(client_fd);

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
