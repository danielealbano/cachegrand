/**
 * Copyright (C) 2018-2022 Vito Castellano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include <cstdbool>
#include <cstring>
#include <memory>
#include <string>
#include <cstdarg>

#include <pthread.h>
#include <mcheck.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include "clock.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "config.h"
#include "fiber/fiber.h"
#include "network/channel/network_channel.h"
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

TestModulesRedisCommandFixture::TestModulesRedisCommandFixture() {
    terminate_event_loop = false;
    char* cpus[] = { "0" };

    config_module_network_binding = {
            .host = "127.0.0.1",
            .port = 12345,
    };

    config_module_redis = {
            .max_key_length = 256,
            .max_command_length = 8 * 1024,
            .max_command_arguments = 128,
    };

    config_module_network_timeout = {
            .read_ms = -1,
            .write_ms = -1,
    };

    config_module_network = {
            .timeout = &config_module_network_timeout,
            .bindings = &config_module_network_binding,
            .bindings_count = 1,
    };

    config_module = {
            .type = CONFIG_MODULE_TYPE_REDIS,
            .network = &config_module_network,
            .redis = &config_module_redis,
    };

    config_network = {
            .backend = CONFIG_NETWORK_BACKEND_IO_URING,
            .max_clients = 10,
            .listen_backlog = 10,
    };

    config_database = {
            .max_keys = 1000,
            .backend = CONFIG_DATABASE_BACKEND_MEMORY,
    };

    config = {
            .cpus = cpus,
            .cpus_count = 1,
            .workers_per_cpus = 1,
            .network = &config_network,
            .modules = &config_module,
            .modules_count = 1,
            .database = &config_database,
    };

    workers_count = config.cpus_count * config.workers_per_cpus;

    db_config = storage_db_config_new();
    db_config->backend_type = STORAGE_DB_BACKEND_TYPE_MEMORY;
    db_config->max_keys = 1000;

    db = storage_db_new(db_config, workers_count);
    storage_db_open(db);

    program_context = program_get_context();
    program_context->config = &config;
    program_context->db = db;

    program_epoch_gc_workers_initialize(&terminate_event_loop, program_context);

    program_config_thread_affinity_set_selected_cpus(program_context);
    program_workers_initialize_count(program_context);
    worker_context = program_workers_initialize_context(
            &terminate_event_loop,
            program_context);

    PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(worker_context, true);

    address = {0};

    client_fd = network_io_common_socket_tcp4_new(0);
    address.sin_family = AF_INET;
    address.sin_port = htons(config_module_network_binding.port);
    address.sin_addr.s_addr = inet_addr(config_module_network_binding.host);

    REQUIRE(connect(client_fd, (struct sockaddr *) &address, sizeof(address)) == 0);
};

TestModulesRedisCommandFixture::~TestModulesRedisCommandFixture() {
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

    program_epoch_gc_workers_cleanup(
            program_context->epoch_gc_workers_context,
            program_context->epoch_gc_workers_count);

    storage_db_close(db);
    storage_db_free(db, workers_count);

    program_reset_context();
}

size_t TestModulesRedisCommandFixture::build_resp_command(
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

size_t TestModulesRedisCommandFixture::string_replace(
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
    auto *replace_len_list = (size_t*)malloc(sizeof(size_t) * count);
    char **with_list = (char**)malloc(sizeof(char*) * count);
    auto *with_len_list = (size_t*)malloc(sizeof(size_t) * count);

    va_start(arg_ptr, count);

    int arg_index = 0;
    while(arg_index < count) {
        replace_list[arg_index] = va_arg(arg_ptr, char *);
        replace_len_list[arg_index] = va_arg(arg_ptr, size_t);
        with_list[arg_index] = va_arg(arg_ptr, char *);
        with_len_list[arg_index] = va_arg(arg_ptr, size_t);
        arg_index++;
    }

    va_end(arg_ptr);

    while(current_char - input < input_len) {
        assert(output - output_begin < output_len);
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

size_t TestModulesRedisCommandFixture::send_recv_resp_command_calculate_multi_recv(
        size_t expected_length) const {
    if (expected_length < recv_packet_size) {
        return 1;
    }

    return 1 + (size_t)ceil((float)expected_length / (float)recv_packet_size) + 1;
}

bool TestModulesRedisCommandFixture::send_recv_resp_command_multi_recv(
        const std::vector<std::string>& arguments,
        char *buffer_recv,
        size_t buffer_recv_length,
        size_t *out_buffer_recv_length,
        int max_recv_count,
        size_t expected_len) const {
    size_t total_send_length, total_recv_length;
    *out_buffer_recv_length = 0;
    memset(buffer_recv, 0, buffer_recv_length);

    // Build the resp command
    size_t buffer_send_length = build_resp_command(nullptr, 0, arguments);
    char *buffer_send_internal = (char *) malloc(buffer_send_length + 1);
    build_resp_command(buffer_send_internal, buffer_send_length + 1, arguments);

    // Send the data
    total_send_length = 0;
    do {
        size_t allowed_buffer_send_length =
                buffer_send_length - total_send_length > NETWORK_CHANNEL_MAX_PACKET_SIZE
                ? NETWORK_CHANNEL_MAX_PACKET_SIZE
                : buffer_send_length - total_send_length;

        assert(allowed_buffer_send_length > 0);
        assert(allowed_buffer_send_length <= NETWORK_CHANNEL_MAX_PACKET_SIZE);

        ssize_t send_length = send(client_fd, buffer_send_internal + total_send_length, allowed_buffer_send_length, MSG_NOSIGNAL);
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

    total_recv_length = 0;
    for(int i = 0; i < max_recv_count && total_recv_length < expected_len; i++) {
        size_t allowed_buffer_recv_size = buffer_recv_length - total_recv_length;
        allowed_buffer_recv_size = allowed_buffer_recv_size > recv_packet_size
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
    }

    *out_buffer_recv_length = total_recv_length;

    return true;
}

bool TestModulesRedisCommandFixture::send_recv_resp_command_multi_recv_and_validate_recv(
        const std::vector<std::string>& arguments,
        char *expected,
        size_t expected_length,
        int max_recv_count) {
    size_t buffer_recv_length = (max_recv_count * recv_packet_size) + 1;
    size_t out_buffer_recv_length = 0;
    char *buffer_recv_internal = (char *)malloc(buffer_recv_length);

    if (!send_recv_resp_command_multi_recv(
            arguments,
            buffer_recv_internal,
            buffer_recv_length,
            &out_buffer_recv_length,
            max_recv_count,
            expected_length)) {
        return false;
    }

    bool recv_matches_expected = memcmp(buffer_recv_internal, expected, min(expected_length, out_buffer_recv_length)) == 0;
    bool return_res = out_buffer_recv_length == expected_length && recv_matches_expected;

    if (!return_res) {
        size_t buffer_send_length = build_resp_command(nullptr, 0, arguments);
        char *buffer_send_internal = (char *) malloc(buffer_send_length + 1);
        build_resp_command(buffer_send_internal, buffer_send_length + 1, arguments);

        char temp_buffer_1[1024] = { 0 };
        char temp_buffer_2[1024] = { 0 };

        memset(temp_buffer_1, 0, sizeof(temp_buffer_1));
        memset(temp_buffer_2, 0, sizeof(temp_buffer_2));
        string_replace(
                buffer_send_internal, buffer_send_length > 64 ? 64 : buffer_send_length,
                temp_buffer_1, sizeof(temp_buffer_1),
                2,
                "\r\n", (size_t) 2, "\\r\\n", (size_t) 4,
                "\0", (size_t) 1, "\\0", (size_t) 2);
        string_replace(
                buffer_send_internal + buffer_send_length - (64 > buffer_send_length ? buffer_send_length : 64), 64 > buffer_send_length ? buffer_send_length : 64,
                temp_buffer_2, sizeof(temp_buffer_2),
                2,
                "\r\n", (size_t) 2, "\\r\\n", (size_t) 4,
                "\0", (size_t) 1, "\\0", (size_t) 2);

        fprintf(
                stderr,
                "[ BUFFER SEND(%ld) ]\n'%s' (%lu)\n'%s' (%lu)\n\n",
                buffer_send_length,
                temp_buffer_1, strlen(temp_buffer_1),
                temp_buffer_2, strlen(temp_buffer_2));

        memset(temp_buffer_1, 0, sizeof(temp_buffer_1));
        memset(temp_buffer_2, 0, sizeof(temp_buffer_2));
        string_replace(
                buffer_recv_internal, out_buffer_recv_length > 64 ? 64 : out_buffer_recv_length,
                temp_buffer_1, sizeof(temp_buffer_1),
                2,
                "\r\n", 2, "\\r\\n", 4,
                "\0", 1, "\\0", 2);
        string_replace(
                buffer_recv_internal + out_buffer_recv_length - (64 > out_buffer_recv_length ? out_buffer_recv_length : 64), 64 > out_buffer_recv_length ? out_buffer_recv_length : 64,
                temp_buffer_2, sizeof(temp_buffer_2),
                2,
                "\r\n", 2, "\\r\\n", 4,
                "\0", 1, "\\0", 2);
        fprintf(
                stderr,
                "[ BUFFER RECV(%ld) ]\n'%s' (%lu)\n'%s' (%lu)\n\n",
                out_buffer_recv_length,
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

        free(buffer_send_internal);
    }

    free(buffer_recv_internal);

    return return_res;
}

bool TestModulesRedisCommandFixture::send_recv_resp_command_and_validate_recv(
        const std::vector<std::string>& arguments,
        char *expected,
        size_t expected_length) {
    return send_recv_resp_command_multi_recv_and_validate_recv(arguments, expected, expected_length, 1);
}

bool TestModulesRedisCommandFixture::send_recv_resp_command_text_and_validate_recv(
        const std::vector<std::string>& arguments,
        char *expected) {
    return send_recv_resp_command_and_validate_recv(arguments, expected, strlen(expected));
}
