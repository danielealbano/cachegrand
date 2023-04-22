/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
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
#include "clock.h"
#include "pow2.h"
#include "log/log.h"
#include "log/log_debug.h"
#include "fatal.h"
#include "xalloc.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "config.h"
#include "fiber/fiber.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "support/io_uring/io_uring_support.h"
#include "support/io_uring/io_uring_capabilities.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "fiber/fiber_scheduler.h"
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
static thread_local bool io_uring_supports_setup_taskrun = false;
static thread_local bool io_uring_supports_single_issuer = false;

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

network_io_common_fd_t worker_iouring_fds_map_get(
        uint32_t fds_map_index,
        worker_iouring_fds_map_files_fd_type_t *type) {
    *type = (worker_iouring_fds_map_files_fd_type_t)(fds_map[fds_map_index] >> 31);
    network_io_common_fd_t fd = fds_map[fds_map_index] & 0x7FFFFFFF;
    return fd;
}

int64_t worker_iouring_fds_map_files_iter(
        uint32_t fds_map_index) {
    for(; fds_map_index < fds_map_count; fds_map_index++) {
        if (fds_map[fds_map_index] != WORKER_FDS_MAP_EMPTY) {
            return fds_map_index;
        }
    }

    return -1;
}

bool worker_iouring_fds_map_files_update(
        io_uring_t *ring,
        int index,
        int fd,
        worker_iouring_fds_map_files_fd_type_t worker_iouring_fds_map_files_fd_type,
        bool *has_mapped_fd,
        int *base_sqe_flags,
        int *wrapped_channel_fd) {
    bool ret;

    fds_map[index] = fd;
    fds_map[index] |= (int)worker_iouring_fds_map_files_fd_type << 31;

    ret = io_uring_register_files_update(
            ring,
            index,
            &fds_map[index],
            1) == 1;

    if (!ret) {
        fds_map[index] = WORKER_FDS_MAP_EMPTY;
        LOG_E(
                TAG,
                "Failed to update the registered fd <%d> with index <%u> in the registered files",
                fd,
                index);
        LOG_E_OS_ERROR(TAG);
    } else {
        *has_mapped_fd = true;
        *base_sqe_flags |= IOSQE_FIXED_FILE;
        *wrapped_channel_fd = index;
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

bool worker_iouring_fds_map_add_and_enqueue_files_update(
        io_uring_t *ring,
        int fd,
        worker_iouring_fds_map_files_fd_type_t worker_iouring_fds_map_files_fd_type,
        bool *has_mapped_fd,
        int *base_sqe_flags,
        int *wrapped_channel_fd) {
    int32_t index;

    if ((index = worker_iouring_fds_map_find_free_index()) < 0) {
        return -1;
    }

    LOG_D(
            TAG,
            "Registering fd <%d> with index <%d>", fd, index);

    if (!worker_iouring_fds_map_files_update(
            ring,
            index,
            fd,
            worker_iouring_fds_map_files_fd_type,
            has_mapped_fd,
            base_sqe_flags,
            wrapped_channel_fd)) {
        return false;
    }

    fds_map_last_free = (index + 1) & fds_map_mask;

    return true;
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

bool worker_iouring_cqe_is_error_any(
        io_uring_cqe_t *cqe) {
    return cqe->res < 0;
}

bool worker_iouring_cqe_is_error(
        io_uring_cqe_t *cqe) {
    // EPIPE, EIO, EBADMSG are not considered by default errors by the scheduler as these can be returned when kTLS is
    // in use and the remote end point suddenly kills the connection without a proper shutdown.
    return
        cqe->res != -ETIME &&
        cqe->res != -EPIPE &&
        cqe->res != -EIO &&
        cqe->res != -EBADMSG &&
        cqe->res != -ECONNRESET &&
        cqe->res != -EAGAIN &&
        cqe->res != -ECANCELED &&
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
    worker_iouring_context_t *iouring_context = worker_context->interface_context;

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

    worker_context->interface_context = NULL;
}

bool worker_iouring_process_events(
        worker_context_t *worker_context) {
    io_uring_cqe_t *cqe;
    worker_iouring_context_t *context;
    fiber_t *fiber;
    uint32_t head, count = 0;

    context = worker_context->interface_context;

    io_uring_support_sqe_submit_and_wait(context->ring, 1);

    io_uring_for_each_cqe(context->ring, head, cqe) {
        count++;
        fiber = (fiber_t*)cqe->user_data;

        // When using link timeout a second CQE is issued that must not be processed so the user data is set to NULL
        // therefore if fiber is set to NULL the cqe will be skipped
        if (fiber == NULL) {
            continue;
        }

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
    io_uring_supports_setup_taskrun = io_uring_capabilities_is_setup_taskrun_supported();
    io_uring_supports_single_issuer = io_uring_capabilities_is_single_issuer_supported();
}

bool worker_iouring_initialize(
        worker_context_t *worker_context,
        uint32_t max_fd,
        uint32_t entries) {
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

        if (io_uring_supports_setup_taskrun) {
            LOG_V(TAG, "io_uring coop taskrun supported and enabled");
            params->flags |= IORING_SETUP_TASKRUN_FLAG | IORING_SETUP_COOP_TASKRUN;
        }

        if (io_uring_supports_single_issuer) {
            LOG_V(TAG, "io_uring single issuer supported and enabled");
            params->flags |= IORING_SETUP_SINGLE_ISSUER;
        }
    }

    context = (worker_iouring_context_t*)xalloc_alloc(sizeof(worker_iouring_context_t));
    worker_context->interface_context = context;
    uint32_t fds_count = max_fd;

    LOG_V(TAG, "Initializing local worker ring for io_uring");

    if ((ring = io_uring_support_init(
            entries,
            params,
            NULL)) == NULL) {
        xalloc_free(params);
        xalloc_free(context);

        return false;
    }

    xalloc_free(params);

    // The iouring context has to be set only after the io_uring is initialized but the actual context memory
    // has to be allocated before, if the allocation fails the software can abort, if the ring is allocated it needs
    // to be cleaned up so better to do it afterwards.
    context->core_index = worker_context->core_index;
    context->ring = ring;
    worker_iouring_context_set(context);

    if (worker_iouring_fds_register(fds_count, ring) == false) {
        io_uring_support_free(ring);
        return false;
    }

    return true;
}
