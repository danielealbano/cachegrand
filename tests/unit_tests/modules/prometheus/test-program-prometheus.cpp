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
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
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

#pragma GCC diagnostic ignored "-Wwrite-strings"

#define PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(WORKER_CONTEXT, RUNNING) { \
    do { \
        sched_yield(); \
        usleep(10000); \
        MEMORY_FENCE_LOAD(); \
    } while((WORKER_CONTEXT)->running == !RUNNING); \
}

struct test_metric_entry {
    char* name;
    bool metric_per_worker;
};
typedef struct test_metric_entry test_metric_entry_t;

TEST_CASE("program.c-prometheus", "[program-prometheus]") {
    worker_context_t *worker_context;
    volatile bool terminate_event_loop = false;
    char* cpus[] = { "0" };

    config_module_network_binding_t config_module_network_binding = {
            .host = "127.0.0.1",
            .port = 12345,
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
            .type = CONFIG_MODULE_TYPE_PROMETHEUS,
            .network = &config_module_network,
    };
    config_network_t config_network = {
            .backend = CONFIG_NETWORK_BACKEND_IO_URING,
            .max_clients = 10,
            .listen_backlog = 10,
    };
    config_database_limits_hard_t config_database_limits_hard = {
            .max_keys = 1000,
    };
    config_database_limits_t config_database_limits = {
            .hard = &config_database_limits_hard,
    };

    config_database_memory_limits_hard_t config_database_memory_limits_hard = {
            .max_memory_usage = 999999999999,
    };
    config_database_memory_limits_t config_database_memory_limits = {
            .hard = &config_database_memory_limits_hard
    };
    config_database_memory_t config_database_memory = {
            .limits = &config_database_memory_limits,
    };
    config_database_t config_database = {
            .limits = &config_database_limits,
            .backend = CONFIG_DATABASE_BACKEND_MEMORY,
            .memory = &config_database_memory
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
    db_config->limits.keys_count.hard_limit = 1000;

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

    PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(worker_context, true)

    struct sockaddr_in address = {0};
    size_t buffer_send_data_len;
    char buffer_send[8 * 1024] = {0};
    char buffer_recv[8 * 1024] = {0};

    int clientfd = network_io_common_socket_tcp4_new(0);
    address.sin_family = AF_INET;
    address.sin_port = htons(config_module_network_binding.port);
    address.sin_addr.s_addr = inet_addr(config_module_network_binding.host);

    REQUIRE(connect(clientfd, (struct sockaddr *) &address, sizeof(address)) == 0);

    SECTION("Prometheus - /metrics endpoint") {
        // Build up the list of the fields in the response
        test_metric_entry_t test_metrics[] = {

                { "cachegrand_uptime", false },
                { "cachegrand_db_keys_count", false },
                { "cachegrand_db_size", false },

                { "cachegrand_network_total_received_packets", true },
                { "cachegrand_network_total_received_data", true },
                { "cachegrand_network_total_sent_packets", true },
                { "cachegrand_network_total_sent_data", true },
                { "cachegrand_network_total_accepted_connections", true },
                { "cachegrand_network_total_active_connections", true },
                { "cachegrand_network_total_accepted_tls_connections", true },
                { "cachegrand_network_total_active_tls_connections", true },
                { "cachegrand_storage_total_written_data", true },
                { "cachegrand_storage_total_write_iops", true },
                { "cachegrand_storage_total_read_data", true },
                { "cachegrand_storage_total_read_iops", true },
                { "cachegrand_storage_total_open_files", true },

                { "cachegrand_network_per_minute_received_packets", true },
                { "cachegrand_network_per_minute_received_data", true },
                { "cachegrand_network_per_minute_sent_packets", true },
                { "cachegrand_network_per_minute_sent_data", true },
                { "cachegrand_network_per_minute_accepted_connections", true },
                { "cachegrand_network_per_minute_accepted_tls_connections", true },
                { "cachegrand_storage_per_minute_written_data", true },
                { "cachegrand_storage_per_minute_write_iops", true },
                { "cachegrand_storage_per_minute_read_data", true },
                { "cachegrand_storage_per_minute_read_iops", true },

                nullptr,
        };

        char request_template[] =
                "GET /metrics HTTP/1.1\r\n"
                "User-Agent: cachegrand-tests\r\n"
                "Host: %s:%d\r\n"
                "Accept: */*\r\n"
                "\r\n";

        snprintf(
                buffer_send,
                sizeof(buffer_send) - 1,
                request_template,
                config_module_network_binding.host,
                config_module_network_binding.port);
        buffer_send_data_len = strlen(buffer_send);

        SECTION("No env vars") {
            char *new_line_ptr;
            int buffer_recv_length;
            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE((buffer_recv_length = recv(clientfd, buffer_recv, sizeof(buffer_recv), 0)) > 0);

            // Check that the response is a 200 OK and that the content type is text plain
            REQUIRE(strncmp(buffer_recv, "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=ASCII\r\n",
                            strlen("HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=ASCII\r\n")) == 0);

            // Search for the end of the HTTP header
            new_line_ptr = buffer_recv;
            while((new_line_ptr = (char*)memchr(
                    new_line_ptr + 1,
                    '\n',
                    (buffer_recv_length - ((new_line_ptr + 1) - buffer_recv))))) {
                if (new_line_ptr - buffer_recv > 3) {
                    if (strncmp(new_line_ptr - 3, "\r\n\r\n", 4) == 0) {
                        break;
                    }
                }
            }

            // Ensure that has found the double new line at the end of the HTTP header
            REQUIRE(new_line_ptr != nullptr);

            for (int test_metric_index = 0; test_metrics[test_metric_index].name != nullptr; test_metric_index++) {
                test_metric_entry_t *test_metric = &test_metrics[test_metric_index];

                // Ensure that there is enough content in the buffer to contain the metric name, "{} ", at least 1 digit
                // and then \n
                REQUIRE((buffer_recv_length - ((new_line_ptr + 1) - buffer_recv)) >= strlen(test_metric->name) + 3 + 1 + 1);
                new_line_ptr++;

                // Ensure that the next metric is the expected one
                REQUIRE(strncmp(new_line_ptr, test_metric->name, strlen(test_metric->name)) == 0);

                // As there are no env labels, ensure that the metric name is followed by "{worker="0"} "
                if (test_metric->metric_per_worker) {
                    REQUIRE(strncmp(new_line_ptr + strlen(test_metric->name), "{worker=\"0\"} ", 13) == 0);
                } else {
                    REQUIRE(strncmp(new_line_ptr + strlen(test_metric->name), "{} ", 3) == 0);
                }

                // THere is always a new line at the end of the line, even the last line
                REQUIRE((new_line_ptr = (char*)memchr(new_line_ptr, '\n', buffer_recv_length)) != nullptr);
            }
        }

        SECTION("With env vars") {
            int buffer_recv_length;
            char *new_line_ptr, *expected_env_labels = R"(label_1="value 1",another_label="another value")";

            // Set 2 env vars to use them as lables in the metrics
            setenv("CACHEGRAND_METRIC_ENV_LABEL_1", "value 1", 1);
            setenv("CACHEGRAND_METRIC_ENV_ANOTHER_LABEL", "another value", 1);

            REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
            REQUIRE((buffer_recv_length = recv(clientfd, buffer_recv, sizeof(buffer_recv), 0)) > 0);

            // Unset the labels before doing anything else
            unsetenv("CACHEGRAND_METRIC_ENV_LABEL_1");
            unsetenv("CACHEGRAND_METRIC_ENV_ANOTHER_LABEL");

            // Check that the response is a 200 OK and that the content type is text plain
            REQUIRE(strncmp(buffer_recv, "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=ASCII\r\n",
                            strlen("HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=ASCII\r\n")) == 0);

            // Search for the end of the HTTP header
            new_line_ptr = buffer_recv;
            while((new_line_ptr = (char*)memchr(
                    new_line_ptr + 1,
                    '\n',
                    (buffer_recv_length - ((new_line_ptr + 1) - buffer_recv))))) {
                if (new_line_ptr - buffer_recv > 3) {
                    if (strncmp(new_line_ptr - 3, "\r\n\r\n", 4) == 0) {
                        break;
                    }
                }
            }

            // Ensure that has found the double new line at the end of the HTTP header
            REQUIRE(new_line_ptr != nullptr);

            char expected_labels_global[255] = { 0 };
            snprintf(
                    expected_labels_global,
                    sizeof(expected_labels_global),
                    "{%s}",
                    expected_env_labels);
            for (int worker_index = 0; worker_index <= 1; worker_index++) {
                char expected_labels_per_worker[255] = { 0 };
                snprintf(
                        expected_labels_per_worker,
                        sizeof(expected_labels_per_worker),
                        "{worker=\"%s\",%s}",
                        worker_index == 0 ? "0" : "aggregated",
                        expected_env_labels);

                for (int test_metric_index = 0; test_metrics[test_metric_index].name != nullptr; ++test_metric_index) {
                    if (worker_index > 0 && !test_metrics[test_metric_index].metric_per_worker) {
                        continue;
                    }

                    test_metric_entry_t *test_metric = &test_metrics[test_metric_index];

                    // Ensure that there is enough content in the buffer to contain the metric name, the labels, a space, at
                    // least 1 digit and then \n
                    REQUIRE((buffer_recv_length - ((new_line_ptr + 1) - buffer_recv)) >=
                            strlen(test_metric->name) + strlen(expected_labels_per_worker) + 1 + 1 + 1);
                    new_line_ptr++;

                    // Ensure that the next metric is the expected one
                    REQUIRE(strncmp(new_line_ptr, test_metric->name, strlen(test_metric->name)) == 0);

                    // As there are no env labels, ensure that the metric name is followed by "{worker="0"} "
                    if (test_metric->metric_per_worker)
                    {
                        // Ensure that the metric name is followed by expected_labels_per_worker
                        REQUIRE(strncmp(
                                new_line_ptr + strlen(test_metric->name),
                                expected_labels_per_worker,
                                strlen(expected_labels_per_worker)) == 0);
                    }
                    else
                    {
                        // Ensure that the metric name is followed by expected_labels
                        REQUIRE(strncmp(
                                new_line_ptr + strlen(test_metric->name),
                                expected_labels_global,
                                strlen(expected_labels_global)) == 0);
                    }

                    // THere is always a new line at the end of the line, even the last line
                    REQUIRE((new_line_ptr = (char*)memchr(new_line_ptr, '\n', buffer_recv_length)) != nullptr);
                }
            }
        }
    }

    SECTION("Prometheus - 404") {
        char request_template[] =
                "GET /random/page.html HTTP/1.1\r\n"
                "User-Agent: cachegrand-tests\r\n"
                "Host: %s:%d\r\n"
                "Accept: */*\r\n"
                "\r\n";

        snprintf(
                buffer_send,
                sizeof(buffer_send) - 1,
                request_template,
                config_module_network_binding.host,
                config_module_network_binding.port);
        buffer_send_data_len = strlen(buffer_send);

        REQUIRE(send(clientfd, buffer_send, buffer_send_data_len, 0) == buffer_send_data_len);
        REQUIRE(recv(clientfd, buffer_recv, sizeof(buffer_recv), 0) > 0);
        REQUIRE(strncmp(buffer_recv, "HTTP/1.1 404 Not Found\r\nContent-Type: text/html; charset=ASCII\r\n",
                        strlen("HTTP/1.1 404 Not Found\r\nContent-Type: text/html; charset=ASCII\r\n")) == 0);
    }

    close(clientfd);

    terminate_event_loop = true;
    MEMORY_FENCE_STORE();

    // Wait for the thread to end
    if (worker_context->running) {
        PROGRAM_WAIT_FOR_WORKER_RUNNING_STATUS(worker_context, false)
        usleep(10000);
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
