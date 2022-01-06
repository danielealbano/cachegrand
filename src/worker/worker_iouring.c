/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <dlfcn.h>
#include <liburing.h>
#include <string.h>
#include <arpa/inet.h>

#include "misc.h"
#include "exttypes.h"
#include "pow2.h"
#include "log/log.h"
#include "log/log_debug.h"
#include "fatal.h"
#include "xalloc.h"
#include "spinlock.h"
#include "config.h"
#include "fiber.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "slab_allocator.h"
#include "support/io_uring/io_uring_support.h"
#include "support/io_uring/io_uring_capabilities.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "worker/worker_common.h"
#include "fiber_scheduler.h"
#include "worker/worker.h"
#include "worker/worker_op.h"
#include "worker/worker_iouring_op.h"
#include "worker/network/worker_network_iouring_op.h"

#include "worker_iouring.h"

#define WORKER_FDS_MAP_EMPTY 0

static thread_local network_io_common_fd_t *fds_map_registered = NULL;
static thread_local network_io_common_fd_t *fds_map = NULL;
static thread_local uint32_t fds_map_count = 0;
static thread_local uint32_t fds_map_mask = 0;
static thread_local uint32_t fds_map_last_free = 0;

static thread_local bool io_uring_supports_op_files_update_link = false;

#define TAG "worker_iouring"

thread_local worker_iouring_context_t *thread_local_worker_iouring_context = NULL;

worker_iouring_context_t* worker_iouring_context_get() {
    return thread_local_worker_iouring_context;
}

void worker_iouring_context_set(
        worker_iouring_context_t *worker_iouring_context) {
    thread_local_worker_iouring_context = worker_iouring_context;
}

void worker_iouring_context_reset() {
    thread_local_worker_iouring_context = NULL;
}

// TODO: need to improve the fiber interface as currently CQEs can be executed only sequentially and having a fiber
//       switch for each connection just to update the fds map would have an hit on the performances for no reason
//bool worker_iouring_op_fds_map_files_update_cb(
//        worker_iouring_context_t *context,
//        worker_iouring_op_context_t *op_context,
//        io_uring_cqe_t *cqe,
//        bool *free_op_context) {
//
//    if (cqe->res < 0) {
//        worker_iouring_fds_map_remove(
//                op_context->io_uring.files_update.channel->mapped_fd);
//        op_context->io_uring.files_update.channel->mapped_fd = WORKER_FDS_MAP_EMPTY;
//    } else {
//        op_context->io_uring.files_update.channel->has_mapped_fd = true;
//        op_context->io_uring.files_update.channel->base_sqe_flags |= IOSQE_FIXED_FILE;
//        op_context->io_uring.files_update.channel->fd =
//                op_context->io_uring.files_update.channel->mapped_fd;
//    }
//
//    return true;
//}

bool worker_iouring_fds_map_files_update(
        io_uring_t *ring,
        int index,
        network_channel_iouring_t *channel) {
    bool ret;

    // TODO: async files update disabled, the fiber interface needs improvements
//    if (io_uring_supports_op_files_update_link) {
//        // TODO: need to fix the implementation in support/io_uring/io_uring_support.c
//        io_uring_sqe_t *sqe = io_uring_support_get_sqe(ring);
//        if (sqe != NULL) {
//            io_uring_prep_files_update(sqe, &fds_map[index], 1, index);
//            io_uring_sqe_set_flags(sqe, IOSQE_IO_LINK);
//            sqe->user_data = (uintptr_t)op_context;
//            ret = true;
//        } else {
//            ret = false;
//        }
//    } else {
        ret = io_uring_register_files_update(
                ring,
                index,
                &fds_map[index],
                1) == 1;
//    }

    if (!ret) {
        LOG_E(
                TAG,
                "Failed to update the registered fd <%d> with index <%u> in the registered files",
                channel->wrapped_channel.fd,
                index);
        LOG_E_OS_ERROR(TAG);
    } else {
        fds_map[index] = channel->wrapped_channel.fd;
        channel->has_mapped_fd = true;
        channel->mapped_fd = index;
    }

    return ret;
}

int32_t worker_iouring_fds_map_find_free_index() {
    int free_fds_map_index = -1;
    for(uint32_t i = 0; i < fds_map_count; i++) {
        uint32_t fds_map_index = (i + fds_map_last_free) & fds_map_mask;
        if (fds_map[fds_map_index] == WORKER_FDS_MAP_EMPTY) {
            free_fds_map_index = (int32_t)fds_map_index;
            break;
        }
    }

    if (free_fds_map_index == -1) {
        LOG_E(
                TAG,
                "Unable to find a free slot for an fd in the fds map");
    }

    assert(free_fds_map_index != -1);

    return free_fds_map_index;
}

int32_t worker_iouring_fds_map_add_and_enqueue_files_update(
        io_uring_t *ring,
        network_channel_iouring_t *channel) {
    int32_t index;

    if ((index = worker_iouring_fds_map_find_free_index()) < 0) {
        return -1;
    }

    LOG_D(
            TAG,
            "Registering fd <%d> with index <%d>", channel->wrapped_channel.fd, index);

    if (!worker_iouring_fds_map_files_update(
            ring,
            index,
            channel)) {
        return -1;
    }

    fds_map_last_free = (index + 1) & fds_map_mask;

    return 0;
}

int worker_iouring_fds_map_remove(
        int index) {
    int fd = fds_map[index];
    fds_map[index] = WORKER_FDS_MAP_EMPTY;

    return fd;
}

bool worker_iouring_fds_register(
        uint32_t fds_count,
        io_uring_t *ring) {

    // pow2_next may return a value greater than UINT32_MAX but it would mean that we are
    // requesting to handle more than 4 billions of FDS with this thread, that can't simply be
    // the case ... like EVER. The caller should ensure this will never happen.
    fds_map_count = pow2_next(fds_count);
    fds_map_mask = fds_map_count - 1;
    fds_map_registered = xalloc_alloc(sizeof(int) * fds_map_count);
    fds_map = xalloc_alloc(sizeof(int) * fds_map_count);

    for(uint32_t fds_map_index = 0; fds_map_index < fds_map_count; fds_map_index++) {
        fds_map[fds_map_index] = WORKER_FDS_MAP_EMPTY;
        fds_map_registered[fds_map_index] = WORKER_FDS_MAP_EMPTY;
    }

    if (io_uring_register_files(ring, fds_map_registered, fds_map_count) < 0) {
        LOG_E(
                TAG,
                "Failed to register the fds_map with io_uring");
        LOG_E_OS_ERROR(TAG);

        io_uring_support_free(ring);

        return false;
    }

    return true;
}

uint32_t worker_iouring_calculate_fds_count(
        uint32_t workers_count,
        uint32_t max_connections,
        uint32_t network_addresses_count) {
    uint32_t max_connections_per_worker = (max_connections / workers_count) * 2;

    return max_connections_per_worker + network_addresses_count;
}

bool worker_iouring_cqe_is_error_any(
        io_uring_cqe_t *cqe) {
    return cqe->res < 0;
}

bool worker_iouring_cqe_is_error(
        io_uring_cqe_t *cqe) {
    return
        cqe->res != -ETIME &&
        cqe->res != -ECONNRESET &&
        cqe->res != -EAGAIN &&
        worker_iouring_cqe_is_error_any(cqe);
}

void worker_iouring_cqe_log(
        io_uring_cqe_t *cqe) {
    assert(cqe->user_data != 0);

    fiber_t *fiber = (fiber_t*)cqe->user_data;

    LOG_E(
            TAG,
            "[FIBER:%s] cqe->user_data = <0x%08llx>, cqe->res = <%s (%d)>, cqe->flags >> 16 = <%d>,"
                " cqe->flags & 0xFFFFu = <%d>",
            fiber->name,
            cqe->user_data,
            cqe->res >= 0 ? "Success" : strerror(cqe->res * -1),
            cqe->res,
            cqe->flags >> 16u,
            cqe->flags & 0xFFFFu);
}

void worker_iouring_cleanup(
        worker_context_t *worker_context) {
    io_uring_t *ring;
    worker_iouring_context_t *iouring_context = worker_iouring_context_get();

    if (iouring_context != NULL) {
        ring = iouring_context->ring;

        // Unregister the files
        io_uring_unregister_files(ring);
        io_uring_support_free(ring);

        // Free up the context
        xalloc_free(iouring_context);
    }

    if (fds_map) {
        xalloc_free(fds_map);
    }

    if (fds_map_registered) {
        xalloc_free((void*)fds_map_registered);
    }
}

bool worker_iouring_process_events_loop(
        worker_context_t *worker_context) {
    io_uring_cqe_t *cqe;
    worker_iouring_context_t *context;
    fiber_t *fiber;
    uint32_t head, count = 0;
    char callback_function_name[256] = { 0 };

    context = worker_iouring_context_get();

    io_uring_support_sqe_submit_and_wait(context->ring, 1);

    io_uring_for_each_cqe(context->ring, head, cqe) {
        count++;
        fiber = (fiber_t*)cqe->user_data;

#if DEBUG == 1
        if (worker_iouring_cqe_is_error(cqe)) {
            worker_iouring_cqe_log(cqe);
        }
#endif

        if (cqe->user_data == 0) {
            FATAL(
                    TAG,
                    "Malformed io_uring cqe, fiber is null!");
        }

        fiber->ret.ptr_value = cqe;
        fiber_scheduler_switch_to(fiber);
    }

    io_uring_support_cq_advance(context->ring, count);

    return true;
}

void worker_iouring_check_capabilities() {
    LOG_V(TAG, "Checking io_uring supported features");
    io_uring_supports_op_files_update_link =
            io_uring_capabilities_is_linked_op_files_update_supported();
}

bool worker_iouring_initialize(
        worker_context_t *worker_context) {
    worker_iouring_context_t *context;
    io_uring_t *ring;
    io_uring_params_t *params = xalloc_alloc_zero(sizeof(io_uring_params_t));

    worker_iouring_check_capabilities();

    if (worker_context->worker_index == 0) {
        char available_features_str[512] = {0};
        LOG_V(
                TAG,
                "io_uring available features: <%s>",
                io_uring_support_features_str(
                        available_features_str,
                        sizeof(available_features_str)));

        if (io_uring_supports_op_files_update_link) {
            LOG_V(TAG, "io_uring linking supported and enabled");
        } else {
            LOG_W(
                    TAG,
                    "io_uring linking not supported, accepting new connections will incur in a"
                        " performance penalty");
        }
    }

    context = (worker_iouring_context_t*)xalloc_alloc(sizeof(worker_iouring_context_t));
    uint32_t fds_count = worker_iouring_calculate_fds_count(
            worker_context->workers_count,
            worker_context->config->network->max_clients,
            worker_context->network.listeners_count);

    LOG_V(TAG, "Initializing local worker ring for io_uring");

    if ((ring = io_uring_support_init(
            fds_count * 1.2,
            params,
            NULL)) == NULL) {
        xalloc_free(context);

        return false;
    }

    // The iouring context has to be set only after the io_uring is initialized but the actual context memory
    // has to be allocated before, if the allocation fails the software can abort, if the ring is allocated it needs
    // to be cleaned up so better to do it afterwards.
    context->worker_context = worker_context;
    context->ring = ring;
    worker_iouring_context_set(context);

    if (worker_iouring_fds_register(fds_count, ring) == false) {
        io_uring_support_free(ring);
        return false;
    }

    return true;
}
