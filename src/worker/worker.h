#ifndef CACHEGRAND_WORKER_H
#define CACHEGRAND_WORKER_H

#ifdef __cplusplus
extern "C" {
#endif

#define WORKER_LOOP_MAX_WAIT_TIME_MS 1000
#define WORKER_LOG_PRODUCER_PREFIX_FORMAT_STRING "[WORKER: %-3u][CPU: %2d][THREAD ID: %-10ld]"
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
        uint16_t max_packet_size;
        uint16_t active_connections;
    } network;
    struct timespec last_update_timestamp;
};
typedef _Volatile(worker_stats_t) worker_stats_volatile_t;

typedef struct worker_user_data worker_user_data_t;
struct worker_user_data {
    pthread_t pthread;
    volatile bool *terminate_event_loop;
    uint32_t workers_count;
    uint32_t worker_index;
    uint32_t core_index;
    config_t *config;
    hashtable_t *hashtable;
    struct {
        worker_stats_t internal;
        worker_stats_volatile_t public;
    } stats;
    struct {
        uint8_t listeners_count;
        network_channel_t *listeners;
        void* context;
    } network;
    void* specialized_user_data;
};

typedef struct worker_op_user_data worker_op_common_user_data_t;
typedef bool (worker_op_completition_cb_t)(
        worker_user_data_t* worker_user_data,
        void* user_data);

struct worker_op_user_data {
    worker_op_completition_cb_t* cb;
    struct {
        network_channel_t *channel;
        network_channel_socket_address_t listener_new_socket_address;
    } network;
    struct {

    } storage;
};

void worker_publish_stats(
        worker_stats_t* worker_stats_new,
        worker_stats_volatile_t* worker_stats_public);

bool worker_should_publish_stats(
        worker_stats_volatile_t* worker_stats_public);

char* worker_log_producer_set_early_prefix_thread(
        worker_user_data_t *worker_user_data);

void worker_setup_user_data(
        worker_user_data_t *worker_user_data,
        uint32_t workers_count,
        uint32_t worker_index,
        volatile bool *terminate_event_loop,
        config_t *config,
        hashtable_t *hashtable);

bool worker_should_terminate(
        worker_user_data_t *worker_user_data);

void worker_request_terminate(
        worker_user_data_t *worker_user_data);

uint32_t worker_thread_set_affinity(
        uint32_t worker_index);

void* worker_thread_func(
        void* user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_H
