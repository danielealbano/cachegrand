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
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker_op.h"
#include "worker/network/worker_network_op.h"
#include "worker/storage/worker_storage_op.h"
#include "network/network.h"
#include "worker/worker_iouring.h"
#include "worker/worker_iouring_op.h"
#include "worker/network/worker_network_iouring_op.h"
#include <worker/storage/worker_storage_iouring_op.h>
#include "worker.h"

#define TAG "worker"

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

bool worker_initialize_general(
        worker_context_t* worker_context) {
    // TODO: the workers should be map the their func ops in a struct and these should be used
    //       below, can't keep doing ifs :/
    if (worker_context->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING ||
        worker_context->config->storage->backend == CONFIG_STORAGE_BACKEND_IO_URING) {

        // TODO: Add some (10) fds for the listeners, this should be calculated dynamically
        uint32_t max_connections_per_worker =
                (worker_context->config->network->max_clients / worker_context->workers_count) + 1 + 10;

        if (!worker_iouring_initialize(worker_context, max_connections_per_worker)) {
            LOG_E(TAG, "io_uring worker initialization failed, terminating");
            worker_iouring_cleanup(worker_context);
            return false;
        }

        worker_iouring_op_register();
    }

    return true;
}

bool worker_initialize_network(
        worker_context_t* worker_context) {
    // TODO: the workers should be map the their func ops in a struct and these should be used
    //       below, can't keep doing ifs :/

    if (worker_context->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING) {
        if (!worker_network_iouring_initialize(worker_context)) {
            LOG_E(TAG, "io_uring worker network initialization failed, terminating");
            worker_iouring_cleanup(worker_context);
            return false;
        }

        worker_network_iouring_op_register();
    }

    return true;
}

bool worker_initialize_storage(
        worker_context_t* worker_context) {
    // TODO: the workers should be map the their func ops in a struct and these should be used
    //       below, can't keep doing ifs :/
    if (worker_context->config->storage->backend == CONFIG_STORAGE_BACKEND_IO_URING) {
        if (!worker_storage_iouring_initialize(worker_context)) {
            LOG_E(TAG, "io_uring worker storage initialization failed, terminating");
            worker_iouring_cleanup(worker_context);
            return false;
        }

        worker_storage_iouring_op_register();
    }

    return true;
}

void worker_cleanup_network(
        worker_context_t* worker_context,
        network_channel_t *listeners,
        uint8_t listeners_count) {
    // TODO: should use a struct with fp pointers, not ifs
    if (worker_context->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING) {
        worker_network_iouring_cleanup(
                listeners,
                listeners_count);
    }

    for(
            uint32_t listener_index = 0; listener_index < listeners_count; listener_index++) {
        network_channel_t *listener_channel = worker_op_network_channel_multi_get(
                listeners,
                listener_index);
        network_io_common_socket_close(
                listener_channel->fd,
                true);
    }

    worker_op_network_channel_free(listeners);
}

void worker_cleanup_storage(
        worker_context_t* worker_context) {
    // TODO: should use a struct with fp pointers, not ifs
    if (worker_context->config->storage->backend == CONFIG_STORAGE_BACKEND_IO_URING) {
        worker_storage_iouring_cleanup(worker_context);
    }

    // TODO: at this point in time there may be data in the buffers waiting to be written and this can lead to potential
    //       data loss / data corruption

    // TODO: Should flush any open fd (device, file or directory) and then close them to ensure data are synced on the
    //       disk
}

void worker_cleanup_general(
        worker_context_t* worker_context) {
    if (worker_context->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING ||
        worker_context->config->storage->backend == CONFIG_STORAGE_BACKEND_IO_URING) {
        worker_iouring_cleanup(worker_context);
    }
}

void worker_mask_signals() {
    sigset_t mask;
    sigfillset(&mask);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

void worker_network_listeners_listen_pre(
        config_network_backend_t backend,
        network_channel_t *listeners,
        uint8_t listeners_count) {
    if (backend == CONFIG_NETWORK_BACKEND_IO_URING) {
        worker_network_iouring_listeners_listen_pre(listeners, listeners_count);
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
    network_channel_t *listeners;
    uint8_t listeners_count;
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
    if (!worker_initialize_general(worker_context)) {
        LOG_E(TAG, "Initialization failed!");
        xalloc_free(log_producer_early_prefix_thread);
        worker_context->aborted = true;
        MEMORY_FENCE_STORE();
        return NULL;
    }

    if (!worker_initialize_network(worker_context)) {
        LOG_E(TAG, "Initialization failed!");
        worker_cleanup_general(worker_context);
        xalloc_free(log_producer_early_prefix_thread);
        worker_context->aborted = true;
        MEMORY_FENCE_STORE();
        return NULL;
    }

    if (!worker_initialize_storage(worker_context)) {
        LOG_E(TAG, "Initialization failed!");
        worker_cleanup_network(worker_context, NULL, 0);
        worker_cleanup_general(worker_context);
        xalloc_free(log_producer_early_prefix_thread);
        worker_context->aborted = true;
        MEMORY_FENCE_STORE();
        return NULL;
    }

    worker_network_listeners_initialize(
            worker_context->core_index,
            worker_context->config->network,
            &listeners,
            &listeners_count);
    worker_network_listeners_listen_pre(
            worker_context->config->network->backend,
            listeners,
            listeners_count);
    worker_network_listeners_listen(
            listeners,
            listeners_count);

    worker_timer_setup(worker_context);

    LOG_I(TAG, "Starting events loop");

    worker_set_running(worker_context, true);

    // TODO: the current loop terminates immediately when requests but this can lead to data corruption while data are
    //       being written by the fibers. To ensure a proper flow of operations the worker should notify the fibers that
    //       they have to terminate the execution ASAP and therefore any network communication should be halted on the
    //       spot but any pending / in progress I/O operation should be safely completed.
    //       In case the fibers are not terminating, even if it can lead to corruption, them should be terminated within
    //       a maximum timeout or X seconds and an error message should be reported pointing out what a fiber is doinng
    //       and where.
    do {
        if (worker_context->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING ||
            worker_context->config->storage->backend == CONFIG_STORAGE_BACKEND_IO_URING) {
            res = worker_iouring_process_events_loop(worker_context);
        }

        if (!res) {
            LOG_E(TAG, "Worker process event loop failed, terminating");
            break;
        }

        if (worker_stats_should_publish(&worker_context->stats.shared)) {
            worker_stats_publish(
                    &worker_context->stats.internal,
                    &worker_context->stats.shared);
        }
    } while(!worker_should_terminate(worker_context));

    LOG_V(TAG, "Process events loop ended, cleaning up");

    worker_cleanup_network(worker_context, listeners, listeners_count);
    worker_cleanup_storage(worker_context);
    worker_cleanup_general(worker_context);

    LOG_V(TAG, "Tearing down");

    xalloc_free(log_producer_early_prefix_thread);

    worker_set_running(worker_context, false);

    return NULL;
}
