/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>

#include "exttypes.h"
#include "misc.h"
#include "fiber.h"
#include "log/log.h"
#include "spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "config.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "worker/worker_common.h"
#include "fiber_scheduler.h"
#include "worker/worker_op.h"

#define TAG "worker_op"

worker_op_timer_fp_t* worker_op_timer;

// Network operations
worker_op_network_channel_new_fp_t* worker_op_network_channel_new;
worker_op_network_channel_multi_new_fp_t* worker_op_network_channel_multi_new;
worker_op_network_channel_multi_get_fp_t* worker_op_network_channel_multi_get;
worker_op_network_channel_size_fp_t* worker_op_network_channel_size;
worker_op_network_channel_free_fp_t* worker_op_network_channel_free;
worker_op_network_accept_fp_t* worker_op_network_accept;
worker_op_network_receive_fp_t* worker_op_network_receive;
worker_op_network_send_fp_t* worker_op_network_send;
worker_op_network_close_fp_t* worker_op_network_close;

// File operations
worker_op_storage_open_fp_t* worker_op_storage_open;
worker_op_storage_read_fp_t* worker_op_storage_read;
worker_op_storage_write_fp_t* worker_op_storage_write;
worker_op_storage_flush_fp_t* worker_op_storage_flush;
worker_op_storage_fallocate_fp_t* worker_op_storage_fallocate;
worker_op_storage_close_fp_t* worker_op_storage_close;

void worker_timer_fiber_entrypoint(
        void *user_data) {
    while(worker_op_timer(0, WORKER_TIMER_LOOP_MS * 1000000l)) {
        // TODO: process timeouts / garbage collector / etc
    }
}

void worker_timer_setup(
        worker_context_t* worker_context) {
    fiber_scheduler_new_fiber(
            "worker-timer",
            sizeof("worker-timer") - 1,
            worker_timer_fiber_entrypoint,
            NULL);
}
