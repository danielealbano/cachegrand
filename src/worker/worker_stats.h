#ifndef CACHEGRAND_WORKER_STATS_H
#define CACHEGRAND_WORKER_STATS_H

#ifdef __cplusplus
extern "C" {
#endif

#define WORKER_PUBLISH_FULL_STATS_INTERVAL_SEC 1

typedef struct worker_stats worker_stats_t;
struct worker_stats {
    struct {
        uint64_t received_packets;
        uint64_t received_data;
        uint64_t sent_packets;
        uint64_t sent_data;
        uint64_t accepted_connections;
        uint16_t active_connections;
        uint64_t accepted_tls_connections;
        uint16_t active_tls_connections;
    } network;
    struct {
        uint64_t written_data;
        uint64_t write_iops;
        uint64_t read_data;
        uint64_t read_iops;
        uint16_t open_files;
    } storage;
    struct timespec started_on_timestamp;
    struct timespec last_update_timestamp;
};
typedef _Volatile(worker_stats_t) worker_stats_volatile_t;

void worker_stats_publish(
        worker_stats_t* worker_stats_new,
        worker_stats_volatile_t* worker_stats_public);

bool worker_stats_should_publish_totals_after_interval(
        worker_stats_volatile_t* worker_stats_public);

worker_stats_t *worker_stats_get_internal_current();

bool worker_stats_get_shared_by_index(
        uint32_t index,
        worker_stats_t *return_worker_stats);

worker_stats_t *worker_stats_aggregate(
        worker_stats_t *aggregated_stats);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_STATS_H
