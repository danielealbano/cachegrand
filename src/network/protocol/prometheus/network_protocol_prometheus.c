/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
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
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "slab_allocator.h"
#include "config.h"
#include "fiber.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"
#include "network/network.h"
#include "signal_handler_thread.h"
#include "program.h"

#include "network_protocol_prometheus.h"

#define TAG "network_protocol_prometheus"

#define NETWORK_PROTOCOL_PROMETHEUS_HTTP_HEADERS_SIZE_INCREASE 5
#define NETWORK_PROTOCOL_PROMETHEUS_HTTP_MAX_URL_LENGTH 256
#define NETWORK_PROTOCOL_PROMETHEUS_HTTP_MAX_HEADER_NAME_LENGTH 256
#define NETWORK_PROTOCOL_PROMETHEUS_HTTP_MAX_HEADER_VALUE_LENGTH (8 * 1024)

const char *metrics_env_prefix = "CACHEGRAND_METRIC_ENV_";
extern char **environ;

typedef struct response_metric_field response_metric_field_t;
struct response_metric_field {
    char *name;
    char *value_formatter;
    uint64_t value;
};

typedef struct client_http_header client_http_header_t;
struct client_http_header {
    char *name;
    char *value;
};

typedef struct client_http_request_data client_http_request_data_t;
struct client_http_request_data {
    char *url;
    size_t url_length;
    bool request_received;
    struct {
        char *current_header_name;
        size_t current_header_name_length;
        uint16_t current_index;
        uint16_t count;
        uint16_t size;
        client_http_header_t *list;
    } headers;
};

typedef struct network_protocol_prometheus_client network_protocol_prometheus_client_t;
struct network_protocol_prometheus_client {
    network_channel_buffer_t read_buffer;
    http_parser_settings http_parser_settings;
    http_parser http_parser;
    client_http_request_data_t http_request_data;
};

void network_protocol_prometheus_client_new(
        network_protocol_prometheus_client_t *network_protocol_prometheus_client,
        config_network_protocol_t *config_network_protocol) {
    network_protocol_prometheus_client->read_buffer.data =
            (char *)slab_allocator_mem_alloc_zero(NETWORK_CHANNEL_RECV_BUFFER_SIZE);
    network_protocol_prometheus_client->read_buffer.length = NETWORK_CHANNEL_RECV_BUFFER_SIZE;
}

void network_protocol_prometheus_client_cleanup(
        network_protocol_prometheus_client_t *network_protocol_prometheus_client) {
    client_http_request_data_t *http_request_data = &network_protocol_prometheus_client->http_request_data;

    if (http_request_data->url) {
        slab_allocator_mem_free(http_request_data->url);
    }

    if (http_request_data->headers.current_header_name) {
        slab_allocator_mem_free(http_request_data->headers.current_header_name);
    }

    if (http_request_data->headers.list) {
        for(uint16_t index = 0; index < http_request_data->headers.count; index++) {
            slab_allocator_mem_free(http_request_data->headers.list[index].name);
            slab_allocator_mem_free(http_request_data->headers.list[index].value);
        }
        slab_allocator_mem_free(http_request_data->headers.list);
    }

    slab_allocator_mem_free(network_protocol_prometheus_client->read_buffer.data);
}

int network_protocol_prometheus_http_parser_on_message_complete(
        http_parser* http_parser) {
    client_http_request_data_t *http_request_data =
            ((client_http_request_data_t*)(http_parser->data));

    http_request_data->request_received = true;
    return 0;
}

int network_protocol_prometheus_http_parser_on_url(
        http_parser* http_parser,
        const char* at, size_t length) {
    client_http_request_data_t *http_request_data =
            ((client_http_request_data_t*)(http_parser->data));

    if (length > NETWORK_PROTOCOL_PROMETHEUS_HTTP_MAX_URL_LENGTH) {
        return -1;
    }

    char *url = slab_allocator_mem_alloc_zero(length + 1);

    if (url == NULL) {
        return -1;
    }

    strncpy(url, at, length);
    url[length] = 0;

    http_request_data->url = url;
    http_request_data->url_length = length;

    return 0;
}

int network_protocol_prometheus_http_parser_on_header_field(
        http_parser* http_parser,
        const char* at, size_t length) {
    client_http_request_data_t *http_request_data =
            ((client_http_request_data_t*)(http_parser->data));

    if (length > NETWORK_PROTOCOL_PROMETHEUS_HTTP_MAX_HEADER_NAME_LENGTH) {
        return -1;
    }

    char *header_name = slab_allocator_mem_alloc_zero(length + 1);

    if (header_name == NULL) {
        return -1;
    }

    strncpy(header_name, at, length);
    header_name[length] = 0;

    http_request_data->headers.current_header_name = header_name;
    http_request_data->headers.current_header_name_length = length;

    return 0;
}

int network_protocol_prometheus_http_parser_on_header_value(
        http_parser* http_parser,
        const char* at, size_t length) {
    client_http_request_data_t *http_request_data =
            ((client_http_request_data_t*)(http_parser->data));

    if (length > NETWORK_PROTOCOL_PROMETHEUS_HTTP_MAX_HEADER_VALUE_LENGTH) {
        return -1;
    }

    // Check if there is enough room to fill out another header in headers.list
    if (http_request_data->headers.count == http_request_data->headers.size) {
        // Expand the list of headers
        size_t headers_list_current_size =
                sizeof(client_http_header_t) * http_request_data->headers.size;
        size_t headers_list_new_size =
                headers_list_current_size +
                (sizeof(client_http_header_t) * NETWORK_PROTOCOL_PROMETHEUS_HTTP_HEADERS_SIZE_INCREASE);
        http_request_data->headers.list =
                slab_allocator_mem_realloc(
                        http_request_data->headers.list,
                        headers_list_current_size,
                        headers_list_new_size,
                        true);
        http_request_data->headers.size += NETWORK_PROTOCOL_PROMETHEUS_HTTP_HEADERS_SIZE_INCREASE;
    }

    char *header_value = slab_allocator_mem_alloc_zero(length + 1);

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

bool network_protocol_prometheus_http_send_response(
        network_channel_t *channel,
        int error_code,
        const char *content_type,
        char *content,
        size_t content_length) {
    size_t http_Response_len;
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
    struct tm tm = *gmtime(&now_timestamp.tv_sec);
    strftime(now, sizeof(now), "%a, %d %b %Y %H:%M:%S %Z", &tm);

    // Calculate the amount of memory needed for the http response
    http_Response_len = snprintf(
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

    http_response = slab_allocator_mem_alloc(http_Response_len + content_length);
    if (!http_response) {
        goto end;
    }

    snprintf(
            http_response,
            http_Response_len + 1,
            http_response_template,
            error_code,
            http_status_str(error_code),
            content_type,
            now,
            CACHEGRAND_CMAKE_CONFIG_NAME,
            CACHEGRAND_CMAKE_CONFIG_VERSION_GIT,
            CACHEGRAND_CMAKE_CONFIG_BUILD_DATE_TIME,
            content_length);

    // To avoid multiple network_send the buffers are squashed together
    strncpy(http_response + http_Response_len, content, content_length);

    result_ret = network_send(channel, http_response, http_Response_len + content_length);

end:
    if (http_response) {
        slab_allocator_mem_free(http_response);
    }

    return result_ret;
}

bool network_protocol_prometheus_http_send_error(
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
    error_message_with_args = slab_allocator_mem_alloc(error_message_with_args_len + 1);
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

    error_html_template = slab_allocator_mem_alloc(error_html_template_len + 1);
    if (!error_html_template) {
        goto end;
    }

    snprintf(
            error_html_template,
            error_html_template_len + 1,
            http_response_error_html_template,
            error_title,
            error_message_with_args);

    result_ret = network_protocol_prometheus_http_send_response(
            channel,
            http_code,
            "text/html; charset=ASCII",
            error_html_template,
            error_html_template_len);

end:
    if (error_message_with_args) {
        slab_allocator_mem_free(error_message_with_args);
    }

    if (error_html_template) {
        slab_allocator_mem_free(error_html_template);
    }

    return result_ret;
}

char *network_protocol_prometheus_fetch_extra_metrics_from_env() {
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
            extra_env_content = slab_allocator_mem_realloc(
                    extra_env_content,
                    extra_env_content_size,
                    extra_env_content_size + 512,
                    false);
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

bool network_protocol_prometheus_process_metrics_request_add_metric(
        char **buffer,
        size_t *length,
        size_t *size,
        const char *name,
        const uint64_t value,
        const char *value_formatter,
        const char *extra_env_metrics) {
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
            extra_env_metrics ? extra_env_metrics : "",
            value);

    if (*length + metric_length + 1 > *size) {
        *buffer = slab_allocator_mem_realloc(
                *buffer,
                *size,
                *size + 128,
                false);

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
            extra_env_metrics ? extra_env_metrics : "",
            value);

    *length += metric_length;

    return true;
}

bool network_protocol_prometheus_process_metrics_request(
        network_channel_t *channel,
        network_protocol_prometheus_client_t *network_protocol_prometheus_client) {
    worker_stats_t aggregated_stats = { 0 };
    timespec_t now = { 0 }, uptime = { 0 };
    char *content = NULL;
    size_t content_length = 0, content_size = 0;
    bool result_ret = false;

    // Aggregate the statistics
    worker_stats_aggregate(&aggregated_stats);

    // Fetch the extra metrics from the env variables
    char *extra_env_content = network_protocol_prometheus_fetch_extra_metrics_from_env();

    // Calculate uptime
    clock_monotonic(&now);
    uptime.tv_sec = now.tv_sec - aggregated_stats.started_on_timestamp.tv_sec;

    // Build up the list of the fields in the response
    response_metric_field_t stats_fields[] = {
        { "network_total_received_packets", "%lu", aggregated_stats.network.total.received_packets },
        { "network_total_received_data", "%lu", aggregated_stats.network.total.received_data },
        { "network_total_sent_packets", "%lu", aggregated_stats.network.total.sent_packets },
        { "network_total_sent_data", "%lu", aggregated_stats.network.total.sent_data },
        { "network_total_accepted_connections", "%lu", aggregated_stats.network.total.accepted_connections },
        { "network_total_active_connections", "%lu", aggregated_stats.network.total.active_connections },
        { "network_total_accepted_tls_connections", "%lu", aggregated_stats.network.total.accepted_tls_connections },
        { "network_total_active_tls_connections", "%lu", aggregated_stats.network.total.active_tls_connections },
        { "storage_total_written_data", "%lu", aggregated_stats.storage.total.written_data },
        { "storage_total_write_iops", "%lu", aggregated_stats.storage.total.write_iops },
        { "storage_total_read_data", "%lu", aggregated_stats.storage.total.read_data },
        { "storage_total_read_iops", "%lu", aggregated_stats.storage.total.read_iops },
        { "storage_total_open_files", "%lu", aggregated_stats.storage.total.open_files },

        { "network_per_minute_received_packets", "%lu", aggregated_stats.network.per_minute.received_packets },
        { "network_per_minute_received_data", "%lu", aggregated_stats.network.per_minute.received_data },
        { "network_per_minute_sent_packets", "%lu", aggregated_stats.network.per_minute.sent_packets },
        { "network_per_minute_sent_data", "%lu", aggregated_stats.network.per_minute.sent_data },
        { "network_per_minute_accepted_connections", "%lu", aggregated_stats.network.per_minute.accepted_connections },
        { "network_per_minute_accepted_tls_connections", "%lu", aggregated_stats.network.per_minute.accepted_tls_connections },
        { "storage_per_minute_written_data", "%lu", aggregated_stats.storage.per_minute.written_data },
        { "storage_per_minute_write_iops", "%lu", aggregated_stats.storage.per_minute.write_iops },
        { "storage_per_minute_read_data", "%lu", aggregated_stats.storage.per_minute.read_data },
        { "storage_per_minute_read_iops", "%lu", aggregated_stats.storage.per_minute.read_iops },

        { "uptime", "%lu", uptime.tv_sec },
        { NULL },
    };

    for(response_metric_field_t *stat_field = stats_fields; stat_field->name; stat_field++) {
        // Build the metrics
        if (!network_protocol_prometheus_process_metrics_request_add_metric(
                &content,
                &content_length,
                &content_size,
                stat_field->name,
                stat_field->value,
                stat_field->value_formatter,
                extra_env_content)) {
            goto end;
        }
    }

    result_ret = network_protocol_prometheus_http_send_response(
        channel,
        200,
        "text/plain; charset=ASCII",
        content,
        content_length);

end:
    if (content) {
        slab_allocator_mem_free(content);
    }

    if (extra_env_content) {
        slab_allocator_mem_free(extra_env_content);
    }

    return result_ret;
}

bool network_protocol_prometheus_process_request(
        network_channel_t *channel,
        network_protocol_prometheus_client_t *network_protocol_prometheus_client) {
    client_http_request_data_t *http_request_data = &network_protocol_prometheus_client->http_request_data;

    if (strlen("/metrics") == http_request_data->url_length && strncmp(http_request_data->url, "/metrics", http_request_data->url_length) == 0) {
        return network_protocol_prometheus_process_metrics_request(
                channel,
                network_protocol_prometheus_client);
    }

    return network_protocol_prometheus_http_send_error(
            channel,
            404,
            "Page not found",
            "The page <b>%.*s</b> doesn't exist",
            (int)http_request_data->url_length,
            http_request_data->url);
}

bool network_protocol_prometheus_process_data(
        network_channel_t *channel,
        network_protocol_prometheus_client_t *network_protocol_prometheus_client) {
    network_channel_buffer_data_t *read_buffer_data_start =
            network_protocol_prometheus_client->read_buffer.data +
            network_protocol_prometheus_client->read_buffer.data_offset;
    size_t data_parsed = http_parser_execute(
            &network_protocol_prometheus_client->http_parser,
            &network_protocol_prometheus_client->http_parser_settings,
            read_buffer_data_start,
            network_protocol_prometheus_client->read_buffer.data_size);

    // Update the buffer cursor
    network_protocol_prometheus_client->read_buffer.data_offset += data_parsed;
    network_protocol_prometheus_client->read_buffer.data_size -= data_parsed;

    if (network_protocol_prometheus_client->http_parser.http_errno != 0) {
        LOG_I(TAG, "Error <%s> parsing http request",
              http_errno_description(network_protocol_prometheus_client->http_parser.http_errno));
        return false;
    }

    if (network_protocol_prometheus_client->http_request_data.request_received) {
        network_protocol_prometheus_process_request(channel, network_protocol_prometheus_client);

        // Always terminate the connection once the request is processed as this implementation is simple enough and
        // doesn't really support pipelining or similar features
        return false;
    }

    return true;
}

void network_protocol_prometheus_accept_setup_http_parser(
        network_protocol_prometheus_client_t *network_protocol_prometheus_client) {
    network_protocol_prometheus_client->http_parser_settings.on_url =
            network_protocol_prometheus_http_parser_on_url;
    network_protocol_prometheus_client->http_parser_settings.on_header_field =
            network_protocol_prometheus_http_parser_on_header_field;
    network_protocol_prometheus_client->http_parser_settings.on_header_value =
            network_protocol_prometheus_http_parser_on_header_value;
    network_protocol_prometheus_client->http_parser_settings.on_message_complete =
            network_protocol_prometheus_http_parser_on_message_complete;

    http_parser_init(&network_protocol_prometheus_client->http_parser, HTTP_REQUEST);
    network_protocol_prometheus_client->http_parser.data = &network_protocol_prometheus_client->http_request_data;
}

void network_protocol_prometheus_accept(
        network_channel_t *channel) {
    bool exit_loop = false;
    network_protocol_prometheus_client_t network_protocol_prometheus_client = { 0 };

    network_protocol_prometheus_client_new(
            &network_protocol_prometheus_client,
            channel->protocol_config);

    network_protocol_prometheus_accept_setup_http_parser(
            &network_protocol_prometheus_client);

    do {
        if (!network_buffer_has_enough_space(
                &network_protocol_prometheus_client.read_buffer,
                NETWORK_CHANNEL_PACKET_SIZE)) {
            exit_loop = true;
        }

        if (!exit_loop) {
            if (network_buffer_needs_rewind(
                    &network_protocol_prometheus_client.read_buffer,
                    NETWORK_CHANNEL_PACKET_SIZE)) {
                network_buffer_rewind(&network_protocol_prometheus_client.read_buffer);
            }

            exit_loop = network_receive(
                    channel,
                    &network_protocol_prometheus_client.read_buffer,
                    NETWORK_CHANNEL_PACKET_SIZE) != NETWORK_OP_RESULT_OK;
        }

        if (!exit_loop) {
            exit_loop = !network_protocol_prometheus_process_data(
                    channel,
                    &network_protocol_prometheus_client);
        }
    } while(!exit_loop);

    network_protocol_prometheus_client_cleanup(&network_protocol_prometheus_client);
}
