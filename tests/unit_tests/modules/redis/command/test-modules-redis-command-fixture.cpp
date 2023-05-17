/**
 * Copyright (C) 2018-2022 Vito Castellano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include <cstdbool>
#include <cstring>
#include <memory>
#include <string>
#include <cstdarg>
#include <cmath>
#include <functional>
#include <algorithm>

#include <pthread.h>
#include <mcheck.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <hiredis.h>

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
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
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
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "epoch_gc.h"
#include "epoch_gc_worker.h"

#include "../../../network/network_tests_support.h"

#include "program.h"

#include "test-modules-redis-command-fixture.hpp"

#pragma GCC diagnostic ignored "-Wwrite-strings"

TestModulesRedisCommandFixture::TestModulesRedisCommandFixture() {
    char* cpus[] = { "0" };

    config_module_network_binding = {
            .host = "127.0.0.1",
            .port = network_tests_support_search_free_port_ipv4(),
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
            .type = "redis",
            .module_id = module_get_by_name("redis")->id,
            .network = &config_module_network,
            .redis = &config_module_redis,
    };

    config_network = {
            .backend = CONFIG_NETWORK_BACKEND_IO_URING,
            .max_clients = 10,
            .listen_backlog = 10,
    };

    config_database_limits_hard = {
            .max_keys = 1000,
    };

    config_database_limits = {
            .hard = &config_database_limits_hard,
    };

    config_database_memory_limits_hard = {
            .max_memory_usage = 999999999999,
    };

    config_database_memory_limits = {
            .hard = &config_database_memory_limits_hard
    };

    config_database_memory = {
            .limits = &config_database_memory_limits,
    };

    config_database_keys_eviction = {
            .only_ttl = false,
            .policy = CONFIG_DATABASE_KEYS_EVICTION_POLICY_RANDOM
    };

    config_database_snapshots = {
            .path = "/tmp/dump.rdb",
            .interval_str = "7d",
            .min_data_changed_str = "0",
            .snapshot_at_shutdown = false,
            .interval_ms = 7 * 86400 * 1000,
            .min_keys_changed = 0,
            .rotation = nullptr,
    };

    config_database = {
            .limits = &config_database_limits,
            .keys_eviction = &config_database_keys_eviction,
            .backend = CONFIG_DATABASE_BACKEND_MEMORY,
            .snapshots = &config_database_snapshots,
            .memory = &config_database_memory,
            .max_user_databases = 16,
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
    db_config->limits.keys_count.hard_limit = 1000;

    program_context = program_get_context();
    program_context->config = &config;
    program_context->workers_count = 1;

    program_config_setup_storage_db(program_context);
    db = program_context->db;
    storage_db_open(db);

    program_epoch_gc_workers_initialize(program_context);

    program_config_thread_affinity_set_selected_cpus(program_context);

    program_workers_initialize_count(program_context);

    worker_context = program_workers_initialize_context(
            program_context);

    PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(worker_context, true)

    this->c = redisConnect(config_module_network_binding.host, config_module_network_binding.port);

    REQUIRE(this->c != nullptr);

    if (this->c->err) {
        fprintf(stderr, "Unable to connect to cachegrand: %s\n", this->c->errstr);
        fflush(stderr);
        REQUIRE(this->c->err == 0);
    }

    // Will be recreated each time during the tests
    redisReaderFree(this->c->reader);
    this->c->reader = nullptr;
}

TestModulesRedisCommandFixture::~TestModulesRedisCommandFixture() {
    // Necessary to avoid a double free
    if (this->c->reader == nullptr) {
        this->c->reader = redisReaderCreate();
    }
    redisFree(this->c);

    // As the config is not allocated via cyaml, set program_context->config to null to avoid the code trying to free
    // memory allocated on the stack
    program_context->config = nullptr;

    // Request the termination of the workers
    program_request_terminate(&program_context->workers_terminate_event_loop);

    // Wait for the cachegrand instance to terminate
    program_cleanup(program_context);

    // Reset the context for the next test if needed
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

bool TestModulesRedisCommandFixture::send_recv_resp_command_multi_recv(
        const std::vector<std::string>& arguments,
        char *buffer_recv_internal,
        size_t *out_buffer_recv_internal_length) const {
    size_t total_recv_length;
    redisReply *reply = nullptr;
    *out_buffer_recv_internal_length = 0;

    // Converts the arguments to a C array of C strings
    std::vector<const char*> c_strs;
    std::transform(std::begin(arguments), std::end(arguments),
                   std::back_inserter(c_strs), std::mem_fn(&std::string::c_str));

    // Sets up the reader
    this->c->reader = redisReaderCreate();

    // Prepares the command and write it to the internal buffer
    redisAppendCommandArgv(
            this->c,
            (int)arguments.size(),
            c_strs.data(),
            nullptr);

    // Sends the data out
    int done = 0;
    do {
        REQUIRE(redisBufferWrite(this->c, &done) == REDIS_OK);
    } while(done == 0);

    // As the redisReaderGetReply discards the reader buffer and we need it entirely instead, the code below handles
    // the receive of the data, store them in an internal buffer and passes them to the reader for processing.
    // Once an element is found the loop terminates, the total length is passed outside and the reply and the reader are
    // freed up.
    total_recv_length = 0;
    do {
        ssize_t recv_length = recv(
                this->c->fd,
                buffer_recv_internal + total_recv_length,
                recv_packet_size,
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

        REQUIRE(redisReaderFeed(this->c->reader, buffer_recv_internal, recv_length) == REDIS_OK);

        if (redisReaderGetReply(this->c->reader, (void**)&reply) != REDIS_OK) {
            char temp_buffer_1[1024] = { 0 };
            string_replace(
                    buffer_recv_internal, total_recv_length,
                    temp_buffer_1, sizeof(temp_buffer_1),
                    2,
                    "\r\n", 2, "\\r\\n", 4,
                    "\0", 1, "\\0", 2);

            fprintf(stderr, "[ HIREDIS ERROR %s (%d) ] \n", this->c->reader->errstr, this->c->reader->err);
            fprintf(stderr, "[ BUFFER CONTENTS ]\n");
            fprintf(stderr, "%s\n", temp_buffer_1);
            fflush(stderr);
            REQUIRE(this->c->reader->err == REDIS_OK);
        }
    } while(reply == nullptr);

    // Return the total length of the received buffer
    *out_buffer_recv_internal_length = total_recv_length;

    // Free up the memory and the reader to reset it for the next test
    freeReplyObject(reply);
    redisReaderFree(this->c->reader);
    this->c->reader = nullptr;

    return true;
}

bool TestModulesRedisCommandFixture::send_recv_resp_command_multi_recv_and_validate_recv(
        const std::vector<std::string>& arguments,
        char *expected,
        size_t expected_length) {
    size_t out_buffer_recv_length = 0;
    char *buffer_recv_internal = (char *)calloc(1, MAX(expected_length, 256));

    send_recv_resp_command_multi_recv(
            arguments,
            buffer_recv_internal,
            &out_buffer_recv_length);

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
    return send_recv_resp_command_multi_recv_and_validate_recv(arguments, expected, expected_length);
}

bool TestModulesRedisCommandFixture::send_recv_resp_command_text_and_validate_recv(
        const std::vector<std::string>& arguments,
        char *expected) {
    return send_recv_resp_command_and_validate_recv(arguments, expected, strlen(expected));
}
