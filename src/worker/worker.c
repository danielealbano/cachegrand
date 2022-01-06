/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#define _GNU_SOURCE

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <liburing.h>
#include <pthread.h>
#include <unistd.h>

#include "exttypes.h"
#include "misc.h"
#include "xalloc.h"
#include "clock.h"
#include "thread.h"
#include "spinlock.h"
#include "memory_fences.h"
#include "utils_numa.h"
#include "log/log.h"
#include "config.h"
#include "support/io_uring/io_uring_support.h"
#include "support/io_uring/io_uring_capabilities.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "worker/worker_common.h"
#include "worker/worker_op.h"
#include "worker/network/worker_network.h"
#include "worker/worker_iouring.h"
#include "worker/worker_iouring_op.h"
#include "worker/network/worker_network_iouring_op.h"
#include <worker/storage/worker_storage_iouring_op.h>
#include "worker.h"

#define TAG "worker"

thread_local worker_context_t *thread_local_worker_context = NULL;

worker_context_t* worker_context_get() {
    return thread_local_worker_context;
}

void worker_context_set(
        worker_context_t *worker_context) {
    thread_local_worker_context = worker_context;
}

void worker_context_reset() {
    thread_local_worker_context = NULL;
}

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
        worker_context_t *worker_context) {
    size_t prefix_size = snprintf(
            NULL,
            0,
            WORKER_LOG_PRODUCER_PREFIX_FORMAT_STRING,
            worker_context->worker_index,
            utils_numa_cpu_current_index()) + 1;
    char *prefix = xalloc_alloc_zero(prefix_size);

    snprintf(
            prefix,
            prefix_size,
            WORKER_LOG_PRODUCER_PREFIX_FORMAT_STRING,
            worker_context->worker_index,
            utils_numa_cpu_current_index());
    log_set_early_prefix_thread(prefix);

    return prefix;
}

void worker_setup_context(
        worker_context_t *worker_context,
        uint32_t workers_count,
        uint32_t worker_index,
        volatile bool *terminate_event_loop,
        config_t *config,
        hashtable_t *hashtable) {
    worker_context->workers_count = workers_count;
    worker_context->worker_index = worker_index;
    worker_context->terminate_event_loop = terminate_event_loop;
    worker_context->config = config;
    worker_context->hashtable = hashtable;
}

bool worker_should_terminate(
        worker_context_t *worker_context) {
    MEMORY_FENCE_LOAD();
    return *worker_context->terminate_event_loop;
}

void worker_request_terminate(
        worker_context_t *worker_context) {
    *worker_context->terminate_event_loop = true;
    MEMORY_FENCE_STORE();
}

uint32_t worker_thread_set_affinity(
        uint32_t worker_index) {
    return thread_current_set_affinity(worker_index);
}

bool worker_initialize(
        worker_context_t* worker_context) {
    // TODO: the workers should be map the their func ops in a struct and these should be used
    //       below, can't keep doing ifs :/
    if (worker_context->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING ||
        worker_context->config->storage->backend == CONFIG_STORAGE_BACKEND_IO_URING) {
        if (!worker_iouring_initialize(worker_context)) {
            LOG_E(TAG, "io_uring worker initialization failed, terminating");
            worker_iouring_cleanup(worker_context);
            return false;
        }

        if (worker_context->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING) {
            if (!worker_network_iouring_initialize(worker_context)) {
                LOG_E(TAG, "io_uring worker network initialization failed, terminating");
                worker_iouring_cleanup(worker_context);
                return false;
            }
            worker_network_iouring_op_register();
        }

        if (worker_context->config->storage->backend == CONFIG_STORAGE_BACKEND_IO_URING) {
            // TODO: worker_storage_iouring_initialize(worker_context)
            // TODO: worker_storage_iouring_op_register();
        }

        worker_iouring_op_register();
    }

    return true;
}

void worker_cleanup(
        worker_context_t* worker_context) {
    // TODO: the network cleanup part should be moved into worker_network as the storage cleanup part should be moved
    //       into worker_storage
    // TODO: should use a struct with fp pointers, not ifs
    if (worker_context->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING ||
        worker_context->config->storage->backend == CONFIG_STORAGE_BACKEND_IO_URING) {
        if (worker_context->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING) {
            worker_network_iouring_cleanup(worker_context);
        }

        if (worker_context->config->storage->backend == CONFIG_STORAGE_BACKEND_IO_URING) {
            // TODO: worker_storage_iouring_cleanup(worker_context)
        }

        worker_iouring_cleanup(worker_context);
    }

    for(
            uint32_t listener_index = 0;
            listener_index < worker_context->network.listeners_count;
            listener_index++) {
        network_channel_t *listener_channel = worker_op_network_channel_multi_get(
                worker_context->network.listeners,
                listener_index);
        network_io_common_socket_close(
                listener_channel->fd,
                true);
    }

    worker_op_network_channel_free(worker_context->network.listeners);
}

void worker_mask_signals() {
    sigset_t mask;
    sigfillset(&mask);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

void worker_network_listeners_listen_pre(
        worker_context_t *worker_context) {
    if (worker_context->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING) {
        worker_network_iouring_listeners_listen_pre(worker_context);
    }
}

void worker_wait_running(
        worker_context_t *worker_context) {
    do {
        pthread_yield();
        usleep(10000);
        MEMORY_FENCE_LOAD();
    } while(!worker_context->running && !worker_context->aborted);
}

void worker_set_running(
    worker_context_t *worker_context,
    bool running) {
    worker_context->running = running;
    MEMORY_FENCE_STORE();
}

void* worker_thread_func(
        void* user_data) {
    bool res = true;
    worker_context_t *worker_context = user_data;

    worker_context->core_index = worker_thread_set_affinity(
            worker_context->worker_index);
    worker_context_set(worker_context);

    char* log_producer_early_prefix_thread =
            worker_log_producer_set_early_prefix_thread(worker_context);

    if (pthread_setname_np(
            pthread_self(),
            "worker") != 0) {
        LOG_E(TAG, "Unable to set name of the worker thread");
        LOG_E_OS_ERROR(TAG);
    }

    worker_mask_signals();

    LOG_I(TAG, "Initialization");
    if (!worker_initialize(worker_context)) {
        LOG_E(TAG, "Initialization failed!");
        xalloc_free(log_producer_early_prefix_thread);
        worker_context->aborted = true;
        MEMORY_FENCE_STORE();
        return NULL;
    }

    worker_network_listeners_initialize(
            worker_context);
    worker_network_listeners_listen_pre(
            worker_context);
    worker_network_listeners_listen(
            worker_context);

    worker_timer_setup(worker_context);

    LOG_I(TAG, "Starting events loop");

    worker_set_running(worker_context, true);

    do {
        if (worker_context->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING ||
            worker_context->config->storage->backend == CONFIG_STORAGE_BACKEND_IO_URING) {
            res = worker_iouring_process_events_loop(worker_context);
        }

        if (!res) {
            LOG_E(TAG, "Worker process event loop failed, terminating");
            break;
        }

        if (worker_should_publish_stats(&worker_context->stats.shared)) {
            worker_publish_stats(
                    &worker_context->stats.internal,
                    &worker_context->stats.shared);
        }
    } while(!worker_should_terminate(worker_context));

    LOG_V(TAG, "Process events loop ended, cleaning up");

    worker_cleanup(worker_context);

    LOG_V(TAG, "Tearing down");

    xalloc_free(log_producer_early_prefix_thread);

    worker_set_running(worker_context, false);

    return NULL;
}
