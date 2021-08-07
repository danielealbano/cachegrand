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

#include "misc.h"
#include "log/log.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "worker/worker_op.h"

worker_op_timer_fp_t* worker_op_timer;
worker_op_network_channel_new_fp_t* worker_op_network_channel_new;
worker_op_network_channel_new_multi_fp_t* worker_op_network_channel_new_multi;
worker_op_network_channel_size_fp_t* worker_op_network_channel_size;
worker_op_network_channel_free_fp_t* worker_op_network_channel_free;
worker_op_network_accept_fp_t* worker_op_network_accept;
worker_op_network_receive_fp_t* worker_op_network_receive;
worker_op_network_send_fp_t* worker_op_network_send;
worker_op_network_close_fp_t* worker_op_network_close;

bool worker_op_timer_completion_cb_loop(
        void* user_data) {
    // TODO: process sockets timeouts and, in general, any other timeout specifically tied to this worker, etc.

    // Resubmit the timer
    return worker_op_timer_loop_submit();
}

bool worker_op_timer_loop_submit() {
    return worker_op_timer(
            worker_op_timer_completion_cb_loop,
            0,
            WORKER_TIMER_LOOP_MS * 1000000l,
            NULL);
}
