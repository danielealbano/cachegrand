#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <config.h>

#include "exttypes.h"
#include "misc.h"
#include "xalloc.h"
#include "thread.h"
#include "memory_fences.h"
#include "utils_numa.h"
#include "log/log.h"
#include "spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"

#include "worker.h"

#define TAG "worker"

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
        LOG_E(TAG, "Unable to fetch the time");
        LOG_E_OS_ERROR(TAG);
        return false;
    }

    return last_update_timestamp.tv_sec >= worker_stats_public->last_update_timestamp.tv_sec + WORKER_PUBLISH_STATS_DELAY_SEC;
}

char* worker_log_producer_set_early_prefix_thread(
        worker_user_data_t *worker_user_data) {
    size_t prefix_size = snprintf(
            NULL,
            0,
            WORKER_LOG_PRODUCER_PREFIX_FORMAT_STRING,
            worker_user_data->worker_index,
            utils_numa_cpu_current_index(),
            thread_current_get_id()) + 1;
    char *prefix = xalloc_alloc_zero(prefix_size);

    snprintf(
            prefix,
            prefix_size,
            WORKER_LOG_PRODUCER_PREFIX_FORMAT_STRING,
            worker_user_data->worker_index,
            utils_numa_cpu_current_index(),
            thread_current_get_id());
    log_set_early_prefix_thread(prefix);

    return prefix;
}

void worker_setup_user_data(
        worker_user_data_t *worker_user_data,
        uint32_t worker_index,
        volatile bool *terminate_event_loop,
        config_t *config,
        hashtable_t *hashtable) {
    worker_user_data->worker_index = worker_index;
    worker_user_data->terminate_event_loop = terminate_event_loop;
    worker_user_data->config = config;
    worker_user_data->hashtable = hashtable;
}

bool worker_should_terminate(
        worker_user_data_t *worker_user_data) {
    HASHTABLE_MEMORY_FENCE_LOAD();
    return *worker_user_data->terminate_event_loop;
}

void worker_request_terminate(
        worker_user_data_t *worker_user_data) {
    *worker_user_data->terminate_event_loop = true;
    HASHTABLE_MEMORY_FENCE_STORE();
}
