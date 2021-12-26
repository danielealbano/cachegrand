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
#include "worker/worker_common.h"
#include "worker/worker_op.h"
#include "worker/worker_fiber_scheduler.h"

#define TAG "worker_op"

worker_op_timer_fp_t* worker_op_timer;
worker_op_network_channel_new_fp_t* worker_op_network_channel_new;
worker_op_network_channel_multi_new_fp_t* worker_op_network_channel_multi_new;
worker_op_network_channel_multi_get_fp_t* worker_op_network_channel_multi_get;
worker_op_network_channel_size_fp_t* worker_op_network_channel_size;
worker_op_network_channel_free_fp_t* worker_op_network_channel_free;
worker_op_network_accept_fp_t* worker_op_network_accept;
worker_op_network_receive_fp_t* worker_op_network_receive;
worker_op_network_send_fp_t* worker_op_network_send;
worker_op_network_close_fp_t* worker_op_network_close;

void worker_timer_fiber_entrypoint() {
    while(worker_op_timer(0, WORKER_TIMER_LOOP_MS * 1000000l)) {
        // TODO: process timeouts / garbage collector / etc
    }
}

void worker_timer_setup(
        worker_context_t* worker_context) {
    char fiber_name[255] = { 0 };
    size_t fiber_name_len = min(
            sizeof(fiber_name),
            snprintf(fiber_name, sizeof(fiber_name),"worker-%d-timer", worker_context->worker_index));

    fiber_t* fiber_timer = fiber_new(
            fiber_name,
            fiber_name_len,
            WORKER_FIBER_STACK_SIZE,
            worker_timer_fiber_entrypoint,
            NULL);

    worker_fiber_scheduler_switch_to(fiber_timer);
}
