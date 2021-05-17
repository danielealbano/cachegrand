/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <config.h>
#include <liburing.h>

#include "exttypes.h"
#include "misc.h"
#include "xalloc.h"
#include "clock.h"
#include "thread.h"
#include "spinlock.h"
#include "memory_fences.h"
#include "utils_numa.h"
#include "log/log.h"
#include "support/io_uring/io_uring_support.h"
#include "support/io_uring/io_uring_capabilities.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"

#include "worker.h"
#include "worker_op.h"
#include "worker_iouring.h"
#include "worker_iouring_op.h"

#define TAG "worker"

void worker_publish_stats(
        worker_stats_t* worker_stats_new,
        worker_stats_volatile_t* worker_stats_public) {
    clock_monotonic(&worker_stats_new->last_update_timestamp);

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

    clock_monotonic(&last_update_timestamp);

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
        uint32_t workers_count,
        uint32_t worker_index,
        volatile bool *terminate_event_loop,
        config_t *config,
        hashtable_t *hashtable) {
    worker_user_data->workers_count = workers_count;
    worker_user_data->worker_index = worker_index;
    worker_user_data->terminate_event_loop = terminate_event_loop;
    worker_user_data->config = config;
    worker_user_data->hashtable = hashtable;
}

bool worker_should_terminate(
        worker_user_data_t *worker_user_data) {
    MEMORY_FENCE_LOAD();
    return *worker_user_data->terminate_event_loop;
}

void worker_request_terminate(
        worker_user_data_t *worker_user_data) {
    *worker_user_data->terminate_event_loop = true;
    MEMORY_FENCE_STORE();
}

bool worker_op_cb_timer_event(
        worker_user_data_t* worker_user_data,
        void* user_data) {
    return worker_op_timer(
            worker_user_data,
            worker_op_cb_timer_event,
            0,
            WORKER_LOOP_MAX_WAIT_TIME_MS * 1000000);
}

uint32_t worker_thread_set_affinity(
        uint32_t worker_index) {
    return thread_current_set_affinity(worker_index);
}

void worker_network_listeners_initialize(
        worker_user_data_t *worker_user_data) {
    network_channel_listener_new_callback_user_data_t listener_new_cb_user_data = {0};
    config_t *config = worker_user_data->config;

    listener_new_cb_user_data.core_index = worker_user_data->core_index;

    // With listeners = NULL, the number of needed listeners will be enumerated and listeners_count
    // increased as needed
    listener_new_cb_user_data.listeners = NULL;
    for(int protocol_index = 0; protocol_index < config->network->protocols_count; protocol_index++) {
        config_network_protocol_t *config_network_protocol = &config->network->protocols[protocol_index];
        for(int binding_index = 0; binding_index < config_network_protocol->bindings_count; binding_index++) {
            if (network_channel_listener_new(
                    config_network_protocol->bindings[binding_index].host,
                    config_network_protocol->bindings[binding_index].port,
                    worker_user_data->config->network->listen_backlog,
                    NETWORK_PROTOCOLS_UNKNOWN,
                    &listener_new_cb_user_data) == false) {
            }
        }
    }

    // Allocate the needed listeners and reset listeners_count
    listener_new_cb_user_data.listeners =
            xalloc_alloc(sizeof(network_channel_t) * listener_new_cb_user_data.listeners_count);
    listener_new_cb_user_data.listeners_count = 0;

    // Allocate the listeners (with the correct protocol config)
    for(int protocol_index = 0; protocol_index < config->network->protocols_count; protocol_index++) {
        network_protocols_t network_protocol;

        config_network_protocol_t *config_network_protocol = &config->network->protocols[protocol_index];
        switch(config_network_protocol->type) {
            default:
            case CONFIG_PROTOCOL_TYPE_REDIS:
                network_protocol = NETWORK_PROTOCOLS_REDIS;
        }

        for(int binding_index = 0; binding_index < config_network_protocol->bindings_count; binding_index++) {
            if (network_channel_listener_new(
                    config_network_protocol->bindings[binding_index].host,
                    config_network_protocol->bindings[binding_index].port,
                    worker_user_data->config->network->listen_backlog,
                    network_protocol,
                    &listener_new_cb_user_data) == false) {

                LOG_E(TAG, "Unable to setup listener for <%s:%u> with protocol <%d>",
                      config_network_protocol->bindings[binding_index].host,
                      config_network_protocol->bindings[binding_index].port,
                      network_protocol);
            }
        }
    }

    // Assign the listeners and listeners_count to the worker user data
    worker_user_data->network.listeners_count = listener_new_cb_user_data.listeners_count;
    worker_user_data->network.listeners = listener_new_cb_user_data.listeners;
}

void* worker_thread_func(
        void* user_data) {
    worker_user_data_t *worker_user_data = user_data;
    worker_user_data->core_index = worker_thread_set_affinity(worker_user_data->worker_index);

    //Set the thread prefix to be used in the logs
    char* log_producer_early_prefix_thread = worker_log_producer_set_early_prefix_thread(worker_user_data);

    LOG_I(TAG, "Worker initialization");

    LOG_V(TAG, "Initializing listeners");
    worker_network_listeners_initialize(
            worker_user_data);

    // TODO: the workers should be map the their func ops in a struct and these should be used
    //       below, can't keep doing ifs :/

    if (worker_user_data->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING ||
        worker_user_data->config->storage->backend == CONFIG_STORAGE_BACKEND_IO_URING) {
        if (!worker_iouring_initialize(worker_user_data)) {
            LOG_E(TAG, "Worker initialization failed, terminating");
            worker_iouring_cleanup(worker_user_data);
            return NULL;
        }

        if (worker_user_data->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING) {
            // TODO: worker_network_iouring_initialize(worker_user_data)
            // TODO: worker_network_iouring_op_register();
        }

        if (worker_user_data->config->storage->backend == CONFIG_STORAGE_BACKEND_IO_URING) {
            // TODO: worker_storage_iouring_initialize(worker_user_data)
            // TODO: worker_storage_iouring_op_register();
        }

        worker_iouring_op_register();
    }

    // Setup timeout
    worker_op_timer(
            worker_user_data,
            worker_op_cb_timer_event,
            0,
            WORKER_LOOP_MAX_WAIT_TIME_MS * 1000000);

    LOG_I(TAG, "Starting worker process events loop");

    do {
        bool res = false;
        if (worker_user_data->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING ||
            worker_user_data->config->storage->backend == CONFIG_STORAGE_BACKEND_IO_URING) {
            res = worker_iouring_process_events_loop(worker_user_data);
        }

        if (!res) {
            LOG_E(TAG, "Worker process event loop failed, terminating");
            break;
        }

        if (worker_should_publish_stats(&worker_user_data->stats.public)) {
            worker_publish_stats(
                    &worker_user_data->stats.internal,
                    &worker_user_data->stats.public);
        }
    } while(!worker_should_terminate(worker_user_data));

    LOG_V(TAG, "Process events loop ended, cleaning up worker");

    if (worker_user_data->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING ||
        worker_user_data->config->storage->backend == CONFIG_STORAGE_BACKEND_IO_URING) {
        worker_iouring_cleanup(worker_user_data);
    }

    LOG_I(TAG, "Tearing down worker");

    xalloc_free(log_producer_early_prefix_thread);

    return NULL;
}
