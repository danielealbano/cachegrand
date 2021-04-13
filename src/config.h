#ifndef CACHEGRAND_CONFIG_H
#define CACHEGRAND_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct config_protocol_bindings config_protocol_bindings_t;
struct config_protocol_bindings {
    char* host;
    char* port;
};

typedef struct config_protocol config_protocol_t;
struct config_protocol {
    network_protocols_t network_protocol_id;
    struct {
        uint32_t connection;
        uint32_t read;
        uint32_t write;
        uint32_t inactivity;
    } timeout;
    struct {
        uint32_t time;
        uint32_t interval;
        uint32_t probes;
    } keepalive;
    config_protocol_bindings_t* bindings;
};

typedef struct config_log_sinks config_log_sinks_t;
struct config_log_sinks {
    // TODO
};

enum config_limits_max_memory_policy {
    CONFIG_LIMITS_MAX_MEMORY_POLICY_TTL_AND_LRU = 0,
    CONFIG_LIMITS_MAX_MEMORY_POLICY_LRU,
    CONFIG_LIMITS_MAX_MEMORY_POLICY_TTL_AND_LFU,
    CONFIG_LIMITS_MAX_MEMORY_POLICY_LFU,
    CONFIG_LIMITS_MAX_MEMORY_POLICY_TTL,
    CONFIG_LIMITS_MAX_MEMORY_POLICY_NO_EVICTION
};
typedef enum config_limits_max_memory_policy config_limits_max_memory_policy_t;

typedef struct config config_t;
struct config {
    config_protocol_t* protocols;
    config_log_sinks_t* log_sinks;
    struct {
        uint32_t max_client;
        uint32_t max_memory;
        config_limits_max_memory_policy_t max_memory_policy;
        uint32_t max_request_length;
    } limits;
    int* cpus;
    bool run_as_service;
    char* pidfile_path;
};

config_t* config_init();
void config_free(config_t* config);
bool config_load(config_t* config, char* config_path, size_t config_path_len);

#ifdef __cplusplus
}
#endif


#endif //CACHEGRAND_CONFIG_H
