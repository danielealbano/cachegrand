#ifndef CACHEGRAND_WORKER_COMMON_H
#define CACHEGRAND_WORKER_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#define WORKER_LOOP_MAX_WAIT_TIME_MS 1000
#define WORKER_PUBLISH_STATS_DELAY_SEC  1

typedef struct worker_stats worker_stats_t;
struct worker_stats {
    struct {
        uint64_t received_packets_per_second;
        uint64_t sent_packets_per_second;
        uint64_t accepted_connections_per_second;
        uint64_t received_packets_total;
        uint64_t sent_packets_total;
        uint64_t accepted_connections_total;
        uint16_t active_connections;
    } network;
    struct timespec last_update_timestamp;
};
typedef _Volatile(worker_stats_t) worker_stats_volatile_t;

typedef struct worker_context worker_context_t;
struct worker_context {
    pthread_t pthread;
    bool_volatile_t *terminate_event_loop;
    bool_volatile_t started;
    uint32_t workers_count;
    uint32_t worker_index;
    uint32_t core_index;
    config_t *config;
    hashtable_t *hashtable;
    struct {
        worker_stats_t internal;
        worker_stats_volatile_t shared;
    } stats;
    struct {
        uint8_t listeners_count;
        network_channel_t *listeners;
        void* context;
    } network;
};

typedef struct worker_network_channel_user_data worker_network_channel_user_data_t;
struct worker_network_channel_user_data {
    bool close_connection_on_send;
    hashtable_t *hashtable;
    size_t packet_size;
    network_channel_buffer_t read_buffer;
    struct {
        network_protocols_t protocol;
        void* context;
    } protocol;
};

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_COMMON_H
