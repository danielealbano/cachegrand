/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include "xalloc.h"
#include "intrinsics.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint64.h"

#include "epoch_operation_queue.h"

epoch_operation_queue_t *epoch_operation_queue_init() {
    epoch_operation_queue_t *epoch_operation_queue = xalloc_alloc_zero(sizeof(epoch_operation_queue_t));
    epoch_operation_queue->ring = ring_bounded_queue_spsc_uint64_init(EPOCH_OPERATION_QUEUE_RING_SIZE);

    return epoch_operation_queue;
}

void epoch_operation_queue_free(
        epoch_operation_queue_t *epoch_operation_queue) {
    ring_bounded_queue_spsc_uint64_free(epoch_operation_queue->ring);
    xalloc_free(epoch_operation_queue);
}

epoch_operation_queue_operation_t *epoch_operation_queue_enqueue(
        epoch_operation_queue_t *epoch_operation_queue) {
    epoch_operation_queue_operation_t operation = {
            .data = {
                    .completed = false,
                    .start_epoch = intrinsics_tsc(),
            }
    };

    epoch_operation_queue_operation_t *operation_ptr =
            (epoch_operation_queue_operation_t *)ring_bounded_queue_spsc_uint64_enqueue_ptr(
                    epoch_operation_queue->ring, operation._packed);

    return operation_ptr;
}

void epoch_operation_queue_mark_completed(
        epoch_operation_queue_operation_t *epoch_operation_queue_operation) {
    epoch_operation_queue_operation->data.completed = true;
}

uint64_t epoch_operation_queue_get_latest_epoch(
        epoch_operation_queue_t *epoch_operation_queue) {
    bool found;
    epoch_operation_queue_operation_t operation;

    do {
        operation._packed = ring_bounded_queue_spsc_uint64_peek(epoch_operation_queue->ring, &found);

        if (!found || operation.data.completed == 0) {
            break;
        }

        ring_bounded_queue_spsc_uint64_dequeue(epoch_operation_queue->ring, NULL);
        epoch_operation_queue->latest_epoch = operation.data.start_epoch;
    } while(true);

    return epoch_operation_queue->latest_epoch;
}
