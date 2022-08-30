/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
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

#include <unistd.h>
#include <netinet/in.h>

#include "clock.h"
#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "config.h"
#include "fiber.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "signal_handler_thread.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"

#include "program.h"

#include "test-modules-redis-command-fixture.hpp"

#pragma GCC diagnostic ignored "-Wwrite-strings"

TEST_CASE_METHOD(TestModulesRedisCommandFixture, "Redis - command - SET", "[redis][command][SET]") {
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
            long_value[i] = ((char)i % range) + 'a';
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
            long_value[i] = ((char)i % range) + 'a';
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