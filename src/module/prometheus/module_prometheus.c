/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <assert.h>
#include <http_parser.h>
#include <ctype.h>
#include <time.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "clock.h"
#include "utils_string.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "config.h"
#include "fiber/fiber.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"
#include "network/network.h"
#include "epoch_gc.h"
#include "epoch_gc_worker.h"
#include "signal_handler_thread.h"
#include "program.h"

#include "module_prometheus.h"

#define TAG "module_prometheus"

const char *metrics_env_prefix = "CACHEGRAND_METRIC_ENV_";
extern char **environ;

FUNCTION_CTOR(module_prometheus_register_ctor, {
    module_register(
            "prometheus",
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            module_prometheus_connection_accept);
});

void module_prometheus_client_new(
        module_prometheus_client_t *module_prometheus_client,
        config_module_t *config_module) {
    module_prometheus_client->read_buffer.data =
            (char *)ffma_mem_alloc_zero(NETWORK_CHANNEL_RECV_BUFFER_SIZE);
    module_prometheus_client->read_buffer.length = NETWORK_CHANNEL_RECV_BUFFER_SIZE;
}

void module_prometheus_client_cleanup(
        module_prometheus_client_t *module_prometheus_client) {
    client_http_request_data_t *http_request_data = &module_prometheus_client->http_request_data;

    if (http_request_data->url) {
        ffma_mem_free(http_request_data->url);
    }

    if (http_request_data->headers.current_header_name) {
        ffma_mem_free(http_request_data->headers.current_header_name);
    }

    if (http_request_data->headers.list) {
        for(uint16_t index = 0; index < http_request_data->headers.count; index++) {
            ffma_mem_free(http_request_data->headers.list[index].name);
            ffma_mem_free(http_request_data->headers.list[index].value);
        }
        ffma_mem_free(http_request_data->headers.list);
    }

    ffma_mem_free(module_prometheus_client->read_buffer.data);
}

int module_prometheus_http_parser_on_message_complete(
        http_parser* http_parser) {
    client_http_request_data_t *http_request_data =
            ((client_http_request_data_t*)(http_parser->data));

    http_request_data->request_received = true;
    return 0;
}

int module_prometheus_http_parser_on_url(
        http_parser* http_parser,
        const char* at, size_t length) {
    client_http_request_data_t *http_request_data =
            ((client_http_request_data_t*)(http_parser->data));

    if (length > MODULE_PROMETHEUS_HTTP_MAX_URL_LENGTH) {
        return -1;
    }

    char *url = ffma_mem_alloc_zero(length + 1);

    if (url == NULL) {
        return -1;
    }

    strncpy(url, at, length);
    url[length] = 0;

    http_request_data->url = url;
    http_request_data->url_length = length;

    return 0;
}

int module_prometheus_http_parser_on_header_field(
        http_parser* http_parser,
        const char* at, size_t length) {
    client_http_request_data_t *http_request_data =
            ((client_http_request_data_t*)(http_parser->data));

    if (length > MODULE_PROMETHEUS_HTTP_MAX_HEADER_NAME_LENGTH) {
        return -1;
    }

    char *header_name = ffma_mem_alloc_zero(length + 1);

    if (header_name == NULL) {
        return -1;
    }

    strncpy(header_name, at, length);
    header_name[length] = 0;

    http_request_data->headers.current_header_name = header_name;
    http_request_data->headers.current_header_name_length = length;

    return 0;
}

int module_prometheus_http_parser_on_header_value(
        http_parser* http_parser,
        const char* at, size_t length) {
    client_http_request_data_t *http_request_data =
            ((client_http_request_data_t*)(http_parser->data));

    if (length > MODULE_PROMETHEUS_HTTP_MAX_HEADER_VALUE_LENGTH) {
        return -1;
    }

    // Check if there is enough room to fill out another header in headers.list
    if (http_request_data->headers.count == http_request_data->headers.size) {
        void *list_new = NULL;

        // Expand the list of headers
        size_t headers_list_current_size =
                sizeof(client_http_header_t) * http_request_data->headers.size;
        size_t headers_list_new_size =
                headers_list_current_size +
                (sizeof(client_http_header_t) * MODULE_PROMETHEUS_HTTP_HEADERS_SIZE_INCREASE);
        list_new = ffma_mem_realloc(http_request_data->headers.list, headers_list_new_size);

        // Check if ffma returned a new slot or if it was able to reuse the old one, if it's a new one the newly
        // allocated memory needs to be freed
        if (unlikely(list_new != http_request_data->headers.list)) {
            // Zero the newly allocated memory
            memset(
                    list_new + headers_list_current_size,
                    0,
                    headers_list_new_size - headers_list_current_size);
        }

        http_request_data->headers.list = list_new;
        http_request_data->headers.size += MODULE_PROMETHEUS_HTTP_HEADERS_SIZE_INCREASE;
    }

    char *header_value = ffma_mem_alloc_zero(length + 1);

    if (header_value == NULL) {
        return -1;
    }

    strncpy(header_value, at, length);
    header_value[length] = 0;

    http_request_data->headers.list[http_request_data->headers.current_index].name =
            http_request_data->headers.current_header_name;
    http_request_data->headers.list[http_request_data->headers.current_index].value = header_value;

    http_request_data->headers.current_index++;
    http_request_data->headers.count++;

    http_request_data->headers.current_header_name = NULL;
    http_request_data->headers.current_header_name_length = 0;

    return 0;
}

bool module_prometheus_http_send_response(
        network_channel_t *channel,
        int error_code,
        const char *content_type,
        char *content,
        size_t content_length) {
    size_t http_response_len;
    bool result_ret = false;
    char *http_response = NULL, now[80];
    timespec_t now_timestamp = { 0 };

    static const char http_response_template[] =
            "HTTP/1.1 %1$d %2$s\r\n"
            "Content-Type: %3$s\r\n"
            "Date: %4$s\r\n"
            "Expires: %4$s\r\n"
            "Server: %5$s-%6$s (built on %7$s)\r\n"
            "Content-Length: %8$lu\r\n"
            "\r\n";

    clock_realtime(&now_timestamp);
    struct tm tm = { 0 };
    gmtime_r(&now_timestamp.tv_sec, &tm);
    strftime(now, sizeof(now), "%a, %d %b %Y %H:%M:%S %Z", &tm);

    // Calculate the amount of memory needed for the http response
    http_response_len = snprintf(
            NULL,
            0,
            http_response_template,
            error_code,
            http_status_str(error_code),
            content_type,
            now,
            CACHEGRAND_CMAKE_CONFIG_NAME,
            CACHEGRAND_CMAKE_CONFIG_VERSION_GIT,
            CACHEGRAND_CMAKE_CONFIG_BUILD_DATE_TIME,
            content_length);

    http_response = ffma_mem_alloc(http_response_len + 1);
    if (!http_response) {
        goto end;
    }

    snprintf(
            http_response,
            http_response_len + 1,
            http_response_template,
            error_code,
            http_status_str(error_code),
            content_type,
            now,
            CACHEGRAND_CMAKE_CONFIG_NAME,
            CACHEGRAND_CMAKE_CONFIG_VERSION_GIT,
            CACHEGRAND_CMAKE_CONFIG_BUILD_DATE_TIME,
            content_length);

    if ((result_ret = network_send_buffered(
            channel,
            http_response,
            http_response_len) == NETWORK_OP_RESULT_OK)) {
        result_ret = network_send_buffered(channel, content, content_length) == NETWORK_OP_RESULT_OK;
    }

end:
    if (http_response) {
        ffma_mem_free(http_response);
    }

    return result_ret;
}

bool module_prometheus_http_send_error(
        network_channel_t *channel,
        int http_code,
        const char* error_title,
        const char* error_message,
        ...) {
    bool result_ret = false;
    char *error_message_with_args = NULL, *error_html_template = NULL;
    size_t error_message_with_args_len, error_html_template_len;
    va_list args, args_copy;

    static const char http_response_error_html_template[] =
            "<!DOCTYPE html>\n<html>\n<head>\n<title>%1$s</title>\n<style>\n"
            "html { color-scheme: light dark; }\nbody { max-width: 40em; margin: 0 auto;\n"
            "font-family: Tahoma, Verdana, Arial, sans-serif; }\n"
            "</style>\n</head>\n<body>\n<h1>%1$s</h1>\n<p>%2$s</p>\n</body>\n</html>";

    va_start(args, error_message);

    // Calculate the error message length
    va_copy(args_copy, args);
    error_message_with_args_len = vsnprintf(NULL, 0, error_message, args_copy);
    va_end(args_copy);

    // Build up the error message with the arguments
    error_message_with_args = ffma_mem_alloc(error_message_with_args_len + 1);
    if (!error_message_with_args) {
        va_end(args);
        goto end;
    }
    vsnprintf(error_message_with_args, error_message_with_args_len + 1, error_message, args);

    va_end(args);

    // Calculate the amount of memory needed for the error template in html
    error_html_template_len = snprintf(
            NULL,
            0,
            http_response_error_html_template,
            error_title,
            error_message_with_args);

    error_html_template = ffma_mem_alloc(error_html_template_len + 1);
    if (!error_html_template) {
        goto end;
    }

    snprintf(
            error_html_template,
            error_html_template_len + 1,
            http_response_error_html_template,
            error_title,
            error_message_with_args);

    result_ret = module_prometheus_http_send_response(
            channel,
            http_code,
            "text/html; charset=ASCII",
            error_html_template,
            error_html_template_len);

end:
    if (error_message_with_args) {
        ffma_mem_free(error_message_with_args);
    }

    if (error_html_template) {
        ffma_mem_free(error_html_template);
    }

    return result_ret;
}

char *module_prometheus_fetch_extra_metrics_from_env() {
    char *extra_env_content = NULL;
    size_t extra_env_content_length = 0;
    size_t extra_env_content_size = 0;
    bool extra_env_metric_found = false;

    for (char **env = environ; *env; ++env) {
        if (strncmp(*env, metrics_env_prefix, strlen(metrics_env_prefix)) != 0) {
            continue;
        }

        char *env_separator = strchr(*env, '=') + 1;
        if (!env_separator) {
            continue;
        }

        size_t metric_env_name_length = strlen(*env) - strlen(metrics_env_prefix) - strlen(env_separator) - 1;
        size_t metric_env_line_length = snprintf(
                NULL,
                0,
                "%s%.*s=\"%s\"",
                extra_env_metric_found ? "," : "",
                (int)metric_env_name_length,
                *env + strlen(metrics_env_prefix),
                env_separator);

        if (extra_env_content_length + metric_env_line_length + 1 > extra_env_content_size) {
            extra_env_content = ffma_mem_realloc(
                    extra_env_content,
                    extra_env_content_size + 512);
            if (!extra_env_content) {
                break;
            }
            extra_env_content_size += 512;
        }

        snprintf(
                extra_env_content + extra_env_content_length,
                metric_env_line_length + 1,
                "%s%.*s=\"%s\"",
                extra_env_metric_found ? "," : "",
                (int)metric_env_name_length,
                *env + strlen(metrics_env_prefix),
                env_separator);

        // Lowercase the name
        char *extra_env_metric_name_start = extra_env_content + extra_env_content_length + (extra_env_metric_found ? 1 : 0);
        for(char* c = extra_env_metric_name_start; c < extra_env_metric_name_start + metric_env_name_length; c++) {
            *c = (char)tolower(*c);
        }

        extra_env_content_length += metric_env_line_length;
        extra_env_content[extra_env_content_length] = 0;
        extra_env_metric_found = true;
    }

    return extra_env_content;
}

bool module_prometheus_process_metrics_request_add_metric(
        char **buffer,
        size_t *length,
        size_t *size,
        const char *name,
        const uint64_t value,
        const char *value_formatter,
        const char *tags) {
    static char *metric_template = "cachegrand_%%s{%%s} %s\n";
    char metric_template_with_value_formatter[256] = { 0 };
    size_t metric_length, metric_template_with_value_formatter_length;

#if DEBUG==1
    metric_template_with_value_formatter_length = snprintf(
            NULL,
            0,
            metric_template,
            value_formatter);

    assert(metric_template_with_value_formatter_length + 1 <= sizeof(metric_template_with_value_formatter));
#endif

    snprintf(
            metric_template_with_value_formatter,
            sizeof(metric_template_with_value_formatter),
            metric_template,
            value_formatter);

    metric_length = snprintf(
            NULL,
            0,
            metric_template_with_value_formatter,
            name,
            tags ? tags : "",
            value);

    if (*length + metric_length + 1 > *size) {
        *buffer = ffma_mem_realloc(
                *buffer,
                *size + 128);

        if (!*buffer) {
            return false;
        }

        *size += 128;
    }

    snprintf(
            *buffer + *length,
            *size - *length,
            metric_template_with_value_formatter,
            name,
            tags ? tags : "",
            value);

    *length += metric_length;

    return true;
}

bool module_prometheus_process_metrics_request(
        network_channel_t *channel,
        module_prometheus_client_t *module_prometheus_client) {
    worker_stats_t worker_stats;
    timespec_t now = { 0 }, uptime = { 0 };
    uint32_t worker_index = 0;
    bool found_worker = false, result_ret = false;
    char *content = NULL, *tags_from_env = NULL, *tags = NULL;
    size_t content_length = 0, content_size = 0;

    // Calculate the uptime
    program_context_t *program_context = program_get_context();
    clock_monotonic(&now);
    clock_diff(
            &now,
            (timespec_t *)&program_context->workers_context[0].stats.shared.started_on_timestamp,
            &uptime);

    // Fetch the extra metrics from the env variables
    tags_from_env = module_prometheus_fetch_extra_metrics_from_env();

    storage_db_counters_t storage_db_counters = { 0 };
    storage_db_counters_sum_global(program_context->db, &storage_db_counters);

    // Send out the global fields
    response_metric_field_t global_stats_fields[] = {
            { "uptime", "%lu", uptime.tv_sec },
            { "db_keys_count", "%lu", storage_db_counters.keys_count },
            { "db_size", "%lu", storage_db_counters.data_size },
            { NULL },
    };

    for(response_metric_field_t *stat_field = global_stats_fields; stat_field->name; stat_field++) {
        // Build the metrics
        if (!module_prometheus_process_metrics_request_add_metric(
                &content,
                &content_length,
                &content_size,
                stat_field->name,
                stat_field->value,
                stat_field->value_formatter,
                tags_from_env)) {
            goto end;
        }
    }

    // Send out the stats for each worker plus the aggregated one
    do {
        size_t tags_len = (tags_from_env ? strlen(tags_from_env) : 0) + 128;
        tags = ffma_mem_alloc(tags_len);

        // Try to fetch the stats for a worker, if it fails it builds up the aggregate, the while will later terminate
        // the loop
        if ((found_worker = worker_stats_get_shared_by_index(
                worker_index,
                &worker_stats)) == false) {
            // Aggregate the statistics
            worker_stats_aggregate(&worker_stats);

            // Build the tags
            snprintf(
                    tags,
                    tags_len,
                    "worker=\"aggregated\"%s%s",
                    tags_from_env ? "," : "",
                    tags_from_env ? tags_from_env : "");
        } else {
            // Build the tags
            snprintf(
                    tags,
                    tags_len,
                    "worker=\"%u\"%s%s",
                    worker_index,
                    tags_from_env ? "," : "",
                    tags_from_env ? tags_from_env : "");
        }

        // Build up the list of the fields in the response
        response_metric_field_t stats_fields[] = {
            { "network_total_received_packets", "%lu", worker_stats.network.total.received_packets },
            { "network_total_received_data", "%lu", worker_stats.network.total.received_data },
            { "network_total_sent_packets", "%lu", worker_stats.network.total.sent_packets },
            { "network_total_sent_data", "%lu", worker_stats.network.total.sent_data },
            { "network_total_accepted_connections", "%lu", worker_stats.network.total.accepted_connections },
            { "network_total_active_connections", "%lu", worker_stats.network.total.active_connections },
            { "network_total_accepted_tls_connections", "%lu", worker_stats.network.total.accepted_tls_connections },
            { "network_total_active_tls_connections", "%lu", worker_stats.network.total.active_tls_connections },
            { "storage_total_written_data", "%lu", worker_stats.storage.total.written_data },
            { "storage_total_write_iops", "%lu", worker_stats.storage.total.write_iops },
            { "storage_total_read_data", "%lu", worker_stats.storage.total.read_data },
            { "storage_total_read_iops", "%lu", worker_stats.storage.total.read_iops },
            { "storage_total_open_files", "%lu", worker_stats.storage.total.open_files },

            { "network_per_minute_received_packets", "%lu", worker_stats.network.per_minute.received_packets },
            { "network_per_minute_received_data", "%lu", worker_stats.network.per_minute.received_data },
            { "network_per_minute_sent_packets", "%lu", worker_stats.network.per_minute.sent_packets },
            { "network_per_minute_sent_data", "%lu", worker_stats.network.per_minute.sent_data },
            { "network_per_minute_accepted_connections", "%lu", worker_stats.network.per_minute.accepted_connections },
            { "network_per_minute_accepted_tls_connections", "%lu", worker_stats.network.per_minute.accepted_tls_connections },
            { "storage_per_minute_written_data", "%lu", worker_stats.storage.per_minute.written_data },
            { "storage_per_minute_write_iops", "%lu", worker_stats.storage.per_minute.write_iops },
            { "storage_per_minute_read_data", "%lu", worker_stats.storage.per_minute.read_data },
            { "storage_per_minute_read_iops", "%lu", worker_stats.storage.per_minute.read_iops },

            { NULL },
        };

        for(response_metric_field_t *stat_field = stats_fields; stat_field->name; stat_field++) {
            // Build the metrics
            if (!module_prometheus_process_metrics_request_add_metric(
                    &content,
                    &content_length,
                    &content_size,
                    stat_field->name,
                    stat_field->value,
                    stat_field->value_formatter,
                    tags)) {
                goto end;
            }
        }

        ffma_mem_free(tags);
        tags = NULL;
        worker_index++;
    } while(found_worker);

    result_ret = module_prometheus_http_send_response(
        channel,
        200,
        "text/plain; charset=ASCII",
        content,
        content_length);

end:
    if (content) {
        ffma_mem_free(content);
    }

    if (tags_from_env) {
        ffma_mem_free(tags_from_env);
    }

    if (tags) {
        ffma_mem_free(tags);
    }

    return result_ret;
}

bool module_prometheus_process_request(
        network_channel_t *channel,
        module_prometheus_client_t *module_prometheus_client) {
    client_http_request_data_t *http_request_data = &module_prometheus_client->http_request_data;

    if (strlen("/metrics") == http_request_data->url_length && strncmp(http_request_data->url, "/metrics", http_request_data->url_length) == 0) {
        return module_prometheus_process_metrics_request(
                channel,
                module_prometheus_client);
    }

    return module_prometheus_http_send_error(
            channel,
            404,
            "Page not found",
            "The page <b>%.*s</b> doesn't exist",
            (int)http_request_data->url_length,
            http_request_data->url);
}

bool module_prometheus_process_data(
        network_channel_t *channel,
        module_prometheus_client_t *module_prometheus_client) {
    network_channel_buffer_data_t *read_buffer_data_start =
            module_prometheus_client->read_buffer.data +
            module_prometheus_client->read_buffer.data_offset;
    size_t data_parsed = http_parser_execute(
            &module_prometheus_client->http_parser,
            &module_prometheus_client->http_parser_settings,
            read_buffer_data_start,
            module_prometheus_client->read_buffer.data_size);

    // Update the buffer cursor
    module_prometheus_client->read_buffer.data_offset += data_parsed;
    module_prometheus_client->read_buffer.data_size -= data_parsed;

    if (module_prometheus_client->http_parser.http_errno != 0) {
        LOG_I(TAG, "Error <%s> parsing http request",
              http_errno_description(module_prometheus_client->http_parser.http_errno));
        return false;
    }

    if (module_prometheus_client->http_request_data.request_received) {
        module_prometheus_process_request(channel, module_prometheus_client);
        if (likely(network_should_flush_send_buffer(channel))) {
            network_flush_send_buffer(channel);
        }
        // Always terminate the connection once the request is processed as this implementation is simple and doesn't
        // really support pipelining or similar features
        return false;
    }

    return true;
}

void module_prometheus_accept_setup_http_parser(
        module_prometheus_client_t *module_prometheus_client) {
    module_prometheus_client->http_parser_settings.on_url =
            module_prometheus_http_parser_on_url;
    module_prometheus_client->http_parser_settings.on_header_field =
            module_prometheus_http_parser_on_header_field;
    module_prometheus_client->http_parser_settings.on_header_value =
            module_prometheus_http_parser_on_header_value;
    module_prometheus_client->http_parser_settings.on_message_complete =
            module_prometheus_http_parser_on_message_complete;

    http_parser_init(&module_prometheus_client->http_parser, HTTP_REQUEST);
    module_prometheus_client->http_parser.data = &module_prometheus_client->http_request_data;
}

void module_prometheus_connection_accept(
        network_channel_t *channel) {
    bool exit_loop = false;
    module_prometheus_client_t module_prometheus_client = { 0 };

    module_prometheus_client_new(
            &module_prometheus_client,
            channel->module_config);

    module_prometheus_accept_setup_http_parser(
            &module_prometheus_client);

    do {
        if (!network_buffer_has_enough_space(
                &module_prometheus_client.read_buffer,
                NETWORK_CHANNEL_MAX_PACKET_SIZE)) {
            exit_loop = true;
        }

        if (!exit_loop) {
            if (network_buffer_needs_rewind(
                    &module_prometheus_client.read_buffer,
                    NETWORK_CHANNEL_MAX_PACKET_SIZE)) {
                network_buffer_rewind(&module_prometheus_client.read_buffer);
            }

            exit_loop = network_receive(
                    channel,
                    &module_prometheus_client.read_buffer,
                    NETWORK_CHANNEL_MAX_PACKET_SIZE) != NETWORK_OP_RESULT_OK;
        }

        if (!exit_loop) {
            exit_loop = !module_prometheus_process_data(
                    channel,
                    &module_prometheus_client);
        }
    } while(!exit_loop);

    module_prometheus_client_cleanup(&module_prometheus_client);
}
