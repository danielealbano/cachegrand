/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
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

#include <mbedtls/aes.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/gcm.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_internal.h>

#include "exttypes.h"
#include "misc.h"
#include "xalloc.h"
#include "clock.h"
#include "thread.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "memory_fences.h"
#include "utils_numa.h"
#include "log/log.h"
#include "config.h"
#include "fiber/fiber.h"
#include "fiber/fiber_scheduler.h"
#include "support/io_uring/io_uring_support.h"
#include "support/io_uring/io_uring_capabilities.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "support/simple_file_io.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker_op.h"
#include "worker/network/worker_network_op.h"
#include "worker/storage/worker_storage_op.h"
#include "network/network.h"
#include "network/network_tls.h"
#include "worker/worker_iouring.h"
#include "worker/worker_iouring_op.h"
#include "worker/network/worker_network_iouring_op.h"
#include "worker/storage/worker_storage_iouring_op.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "worker.h"
#include "worker/worker_fiber.h"
#include "worker/fiber/worker_fiber_storage_db_gc_deleted_entries.h"
#include "worker/fiber/worker_fiber_storage_db_initialize.h"
#include "worker/fiber/worker_fiber_storage_db_keys_eviction.h"
#include "worker/fiber/worker_fiber_storage_db_snapshot_rdb.h"

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
        timespec_t *started_on_timestamp,
        uint32_t workers_count,
        uint32_t worker_index,
        bool_volatile_t *terminate_event_loop,
        bool_volatile_t *storage_db_loaded,
        config_t *config,
        storage_db_t *db) {
    worker_context->workers_count = workers_count;
    worker_context->worker_index = worker_index;
    worker_context->terminate_event_loop = terminate_event_loop;
    worker_context->config = config;
    worker_context->db = db;
    worker_context->aborted = false;
    worker_context->running = false;
    worker_context->storage_db_loaded = storage_db_loaded;

    worker_context->stats.internal.started_on_timestamp.tv_nsec = started_on_timestamp->tv_nsec;
    worker_context->stats.internal.started_on_timestamp.tv_sec = started_on_timestamp->tv_sec;
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
    if (!worker_fiber_init(worker_context)) {
        return false;
    }

    // TODO: the backends should map the their funcs in a struct and these should be used below, can't keep doing ifs :/
    if (worker_context->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING ||
        worker_context->config->database->backend == CONFIG_DATABASE_BACKEND_FILE) {

        // TODO: Add some (10) fds for the listeners, plenty but this should be calculated dynamically
        uint32_t max_connections_per_worker =
                (uint32_t)(((double)worker_context->config->network->max_clients * 1.2f) / (double)worker_context->workers_count) + 1 + 10;

        // The amount of entries has to be double the amount of connections because of the timeouts' management (extra
        // sqe for LINK_TIMEOUT for reads and writes)
        if (!worker_iouring_initialize(
                worker_context,
                max_connections_per_worker,
                max_connections_per_worker * 2)) {
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
    if (!worker_storage_iouring_initialize(worker_context)) {
        LOG_E(TAG, "io_uring worker storage initialization failed, terminating");
        worker_iouring_cleanup(worker_context);
        return false;
    }

    worker_storage_iouring_op_register();

    return true;
}

void worker_shutdown_network_listeners(
        network_channel_t *listeners,
        uint8_t listeners_count) {
    for(
            uint32_t listener_index = 0; listener_index < listeners_count; listener_index++) {
        network_channel_t *listener_channel = worker_op_network_channel_multi_get(
                listeners,
                listener_index);
        network_io_common_socket_close(
                listener_channel->fd,
                true);
    }
}

void worker_cleanup_network(
        worker_context_t* worker_context,
        network_channel_t *listeners,
        uint8_t listeners_count) {
    // TODO: should use a struct with fp pointers, not ifs
    if (worker_context->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING) {
        worker_network_iouring_cleanup(listeners, listeners_count);
    }

    if (listeners) {
        worker_op_network_channel_multi_free(listeners, listeners_count);
    }
}

void worker_cleanup_storage(
        worker_context_t* worker_context) {
    // TODO: should use a struct with fp pointers, not ifs
    if (worker_context->config->database->backend == CONFIG_DATABASE_BACKEND_FILE) {
        worker_storage_iouring_cleanup(worker_context); // lgtm [cpp/useless-expression]
    }

    // TODO: at this point in time there may be data in the buffers waiting to be written and this can lead to potential
    //       data loss / data corruption

    // TODO: Should flush any open fd (device, file or directory) and then close them to ensure data are synced on the
    //       disk
}

void worker_cleanup_general(
        worker_context_t* worker_context) {
    if (worker_context->config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING ||
        worker_context->config->database->backend == CONFIG_DATABASE_BACKEND_FILE) {
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
    int loops = 0;
    do {
        sched_yield();
        usleep(10000);
        MEMORY_FENCE_LOAD();
        loops++;
    } while(!worker_context->running && !worker_context->aborted);
}

void worker_set_running(
        worker_context_t *worker_context,
        bool running) {
    worker_context->running = running;
    MEMORY_FENCE_STORE();
}

void worker_set_aborted(
        worker_context_t *worker_context,
        bool aborted) {
    worker_context->aborted = aborted;
    MEMORY_FENCE_STORE();
}

bool worker_initialize_storage_db(
        worker_context_t *worker_context) {
    // No need to keep track of this fiber, it will be freed right away once the control is returned to the code
    if (!worker_fiber_register(
            worker_context,
            "worker-fiber-storage-db-initialize",
            worker_fiber_storage_db_initialize_fiber_entrypoint,
            (fiber_scheduler_new_fiber_user_data_t *)worker_context->storage_db_loaded)) {
        return false;
    }

    if (!worker_fiber_register(
            worker_context,
            "worker-fiber-storage-db-gc-deleted-entries",
            worker_fiber_storage_db_gc_deleted_entries_fiber_entrypoint,
            NULL)) {
        return false;
    }

    if (!worker_fiber_register(
            worker_context,
            "worker-fiber-storage-db-keys-eviction",
            worker_fiber_storage_db_keys_eviction_fiber_entrypoint,
            NULL)) {
        return false;
    }

    if (!worker_fiber_register(
            worker_context,
            "worker-fiber-storage-db-snapshot-rdb",
            worker_fiber_storage_db_snapshot_rdb_fiber_entrypoint,
            NULL)) {
        return false;
    }

    return true;
}

void worker_cleanup(
        worker_context_t *worker_context,
        char* log_producer_early_prefix_thread,
        network_channel_t *listeners,
        uint8_t listeners_count,
        worker_module_context_t *worker_module_contexts,
        bool aborted) {
    worker_fiber_free(
            worker_context);

    worker_module_context_free(
            worker_context->config,
            worker_module_contexts);

    worker_cleanup_network(
            worker_context,
            listeners,
            listeners_count);

    worker_cleanup_storage(
            worker_context);

    worker_cleanup_general(
            worker_context);

    fiber_scheduler_free();

    xalloc_free(log_producer_early_prefix_thread);

    worker_set_running(worker_context, false);
    worker_set_aborted(worker_context, aborted);
    MEMORY_FENCE_STORE();
}

void* worker_thread_func(
        void* user_data) {
    bool aborted = true;
    bool res = false;
    worker_module_context_t *worker_module_contexts = NULL;
    network_channel_t *listeners = NULL;
    uint8_t listeners_count = 0;
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

    LOG_V(TAG, "Initialization");

    if (!worker_initialize_general(worker_context)) {
        LOG_E(TAG, "Initialization failed!");
        goto end;
    }

    if (!worker_initialize_network(worker_context)) {
        LOG_E(TAG, "Unable to initialize the network subsystem!");
        goto end;
    }

    if (!worker_initialize_storage(worker_context)) {
        LOG_E(TAG, "Unable to initialize the storage subsystem!");
        goto end;
    }

    if (worker_initialize_storage_db(worker_context) == false) {
        LOG_E(TAG, "Unable to initialize the database!");
        goto end;
    }

    if ((worker_module_contexts = worker_module_contexts_initialize(
            worker_context->config)) == NULL) {
        LOG_E(TAG, "Unable to initialize the listeners!");
        goto end;
    }

    LOG_V(TAG, "Starting events loop");

    // TODO: the current loop terminates immediately when requests but this can lead to data corruption while data are
    //       being written by the fibers. To ensure a proper flow of operations the worker should notify the fibers that
    //       they have to terminate the execution ASAP and therefore any network communication should be halted on the
    //       spot but any pending / in progress I/O operation should be safely completed.
    //       In case the fibers are not terminating, even if it can lead to corruption, them should be terminated within
    //       a maximum timeout or X seconds and an error message should be reported pointing out what a fiber is doing
    //       and where.
    bool can_terminate_loop = false;
    bool can_start_snapshot_at_shutdown = false;
    bool snapshot_at_shutdown_started = false;
    bool shutdown_done_waiting_for_storage_fds = false;
    uint64_t snapshot_shutdown_start_time = 0;
    do {
        // Check if the database has been loaded
        if (worker_context->running == false && *worker_context->storage_db_loaded == true) {
            if (!worker_network_listeners_initialize(
                    worker_context->worker_index,
                    worker_context->core_index,
                    worker_context->config,
                    worker_module_contexts,
                    &listeners,
                    &listeners_count)) {
                LOG_E(TAG, "Unable to initialize the listeners!");
                goto end;
            }

            worker_network_listeners_listen_pre(
                    worker_context->config->network->backend,
                    listeners,
                    listeners_count);

            // Starts to listen and marks the worker as running
            worker_network_listeners_listen(
                    worker_context,
                    listeners,
                    listeners_count);

            worker_set_running(worker_context, true);
        }

        // Check if the work can terminate
        if (worker_should_terminate(worker_context)) {
            if (shutdown_done_waiting_for_storage_fds == false) {
                // Close all the listeners
                worker_shutdown_network_listeners(
                        listeners,
                        listeners_count);

                // Close all the network sockets
                int64_t fds_map_files_index = -1;
                while((fds_map_files_index = worker_iouring_fds_map_files_iter(fds_map_files_index + 1)) != -1) {
                    worker_iouring_fds_map_files_fd_type_t type;
                    network_io_common_fd_t fd = worker_iouring_fds_map_get(fds_map_files_index, &type);

                    if (type != WORKER_FDS_MAP_FILES_FD_TYPE_NETWORK_CHANNEL) {
                        continue;
                    }

                    network_io_common_socket_close(fd, true);
                }

                // Mark that it has to wait
                shutdown_done_waiting_for_storage_fds = true;
            } else {
                // Wait for all the fds to be closed
                uint32_t counter = 0;
                int64_t fds_map_files_index = -1;
                while((fds_map_files_index = worker_iouring_fds_map_files_iter(fds_map_files_index + 1)) != -1) {
                    counter++;
                }

                if (counter == 0) {
                    // All the fds have been closed
                    if (worker_context->db->config->snapshot.snapshot_at_shutdown) {
                        can_start_snapshot_at_shutdown = true;
                    } else {
                        can_terminate_loop = true;
                    }
                }
            }

            if (can_start_snapshot_at_shutdown && !snapshot_at_shutdown_started) {
                MEMORY_FENCE_LOAD();
                if (worker_context->db->snapshot.running == false) {
                    snapshot_shutdown_start_time = clock_monotonic_int64_ms();
                    worker_context->db->snapshot.next_run_time_ms = snapshot_shutdown_start_time - 1;
                    MEMORY_FENCE_STORE();
                    snapshot_at_shutdown_started = true;
                }
            }

            if (snapshot_at_shutdown_started) {
                MEMORY_FENCE_LOAD();
                if (worker_context->db->snapshot.next_run_time_ms > snapshot_shutdown_start_time) {
                    can_terminate_loop = true;
                }
            }
        }

        // Process events
        res = worker_iouring_process_events(worker_context);

        if (!res) {
            LOG_E(TAG, "Worker process event loop failed, terminating");
            break;
        }

        // Checks if the stats should be published with an interval of 1 second
        if (worker_stats_should_publish_totals_after_interval(&worker_context->stats.shared)) {
            // Check if the per_minute stats should be published too
            bool only_total = !worker_stats_should_publish_per_minute_after_interval(
                    &worker_context->stats.shared);
            worker_stats_publish(
                    &worker_context->stats.internal,
                    &worker_context->stats.shared,
                    only_total);
        }
    } while(!can_terminate_loop);

    aborted = false;
    end:
    LOG_V(TAG, "Worker events loop ended, cleaning up");

    worker_cleanup(
            worker_context,
            log_producer_early_prefix_thread,
            listeners,
            listeners_count,
            worker_module_contexts,
            aborted);

    return NULL;
}
