#ifndef CACHEGRAND_MODULE_PROMETHEUS_H
#define CACHEGRAND_MODULE_PROMETHEUS_H

#ifdef __cplusplus
extern "C" {
#endif

#define MODULE_PROMETHEUS_HTTP_HEADERS_SIZE_INCREASE 5
#define MODULE_PROMETHEUS_HTTP_MAX_URL_LENGTH 256
#define MODULE_PROMETHEUS_HTTP_MAX_HEADER_NAME_LENGTH 256
#define MODULE_PROMETHEUS_HTTP_MAX_HEADER_VALUE_LENGTH (8 * 1024)

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

typedef struct module_prometheus_client module_prometheus_client_t;
struct module_prometheus_client {
    network_channel_buffer_t read_buffer;
    http_parser_settings http_parser_settings;
    http_parser http_parser;
    client_http_request_data_t http_request_data;
};

void module_prometheus_connection_accept(
        network_channel_t *channel);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_PROMETHEUS_H
