#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include "exttypes.h"
#include "misc.h"
#include "xalloc.h"
#include "log.h"
#include "network/channel/network_channel.h"

#include "worker.h"

LOG_PRODUCER_CREATE_DEFAULT("worker", worker)

void worker_publish_stats(
        worker_stats_t* worker_stats_new,
        worker_stats_volatile_t* worker_stats_public) {
    clock_gettime(CLOCK_MONOTONIC, &worker_stats_new->last_update_timestamp);

    memcpy((void*)&worker_stats_public->network, &worker_stats_new->network, sizeof(worker_stats_public->network));
    worker_stats_public->last_update_timestamp.tv_nsec = worker_stats_new->last_update_timestamp.tv_nsec;
    worker_stats_public->last_update_timestamp.tv_sec = worker_stats_new->last_update_timestamp.tv_sec;

    worker_stats_new->network.received_packets_per_second = 0;
    worker_stats_new->network.sent_packets_per_second = 0;
    worker_stats_new->network.accepted_connections_per_second = 0;
}

bool worker_should_publish_stats(
        worker_stats_volatile_t* worker_stats_public) {
    struct timespec last_update_timestamp;

    if (clock_gettime(CLOCK_MONOTONIC, &last_update_timestamp) < 0) {
        LOG_E(LOG_PRODUCER_DEFAULT, "Unable to fetch the time");
        LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);
        return false;
    }

    return last_update_timestamp.tv_sec >= worker_stats_public->last_update_timestamp.tv_sec + WORKER_PUBLISH_STATS_DELAY_SEC;
}

char* worker_log_producer_set_early_prefix_thread(
        worker_user_data_t *worker_user_data) {
    pid_t tid = syscall(SYS_gettid);
    size_t prefix_size = strlen(WORKER_LOG_PRODUCER_PREFIX_FORMAT_STRING) + 50 + 1;
    char *prefix = xalloc_alloc_zero(prefix_size);

    int res = snprintf(
            prefix,
            prefix_size,
            WORKER_LOG_PRODUCER_PREFIX_FORMAT_STRING,
            worker_user_data->worker_index,
            tid);
    log_producer_set_early_prefix_thread(prefix);

    return prefix;
}

void worker_setup_user_data(
        worker_user_data_t *worker_user_data,
        uint32_t worker_index,
        volatile bool *terminate_event_loop,
        uint32_t max_connections,
        uint32_t backlog,
        network_channel_address_t* addresses,
        uint32_t addresses_count) {
    worker_user_data->worker_index = worker_index;
    worker_user_data->terminate_event_loop = terminate_event_loop;
    worker_user_data->max_connections = max_connections;
    worker_user_data->backlog = backlog;
    worker_user_data->addresses_count = addresses_count;
    worker_user_data->addresses = (network_channel_address_t*)addresses;
}
