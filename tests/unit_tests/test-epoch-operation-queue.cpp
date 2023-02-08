/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include "intrinsics.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint64.h"

#include "epoch_operation_queue.h"

TEST_CASE("epoch_operation_queue.c", "[epoch_operation_queue]") {
    SECTION("epoch_operation_queue_init") {
        epoch_operation_queue_t *epoch_operation_queue = epoch_operation_queue_init();

        REQUIRE(epoch_operation_queue != nullptr);
        REQUIRE(epoch_operation_queue->latest_epoch == 0);
        REQUIRE(epoch_operation_queue->ring != nullptr);

        epoch_operation_queue_free(epoch_operation_queue);
    }

    SECTION("epoch_operation_queue_enqueue") {
        epoch_operation_queue_t *epoch_operation_queue = epoch_operation_queue_init();

        SECTION("enqueue one") {
            epoch_operation_queue_operation_t *op = epoch_operation_queue_enqueue(epoch_operation_queue);

            REQUIRE(op != nullptr);
            REQUIRE(op->data.start_epoch > 0);
            REQUIRE(op->data.start_epoch < intrinsics_tsc());
            REQUIRE(op->data.completed == 0);
            REQUIRE(&op->_packed == &epoch_operation_queue->ring->items[0]);
            REQUIRE(ring_bounded_queue_spsc_uint64_get_length(epoch_operation_queue->ring) == 1);
        }

        SECTION("enqueue two") {
            epoch_operation_queue_operation_t *op1 = epoch_operation_queue_enqueue(epoch_operation_queue);
            epoch_operation_queue_operation_t *op2 = epoch_operation_queue_enqueue(epoch_operation_queue);

            REQUIRE(op2 != nullptr);
            REQUIRE(op2->data.start_epoch > op1->data.start_epoch);
            REQUIRE(op2->data.start_epoch < intrinsics_tsc());
            REQUIRE(op2->data.completed == false);
            REQUIRE(&op2->_packed == &epoch_operation_queue->ring->items[1]);
            REQUIRE(ring_bounded_queue_spsc_uint64_get_length(epoch_operation_queue->ring) == 2);
        }

        SECTION("fill queue") {
            for(int i = 0; i < EPOCH_OPERATION_QUEUE_RING_SIZE; i++) {
                epoch_operation_queue_enqueue(epoch_operation_queue);
            }

            REQUIRE(ring_bounded_queue_spsc_uint64_get_length(epoch_operation_queue->ring) ==
                EPOCH_OPERATION_QUEUE_RING_SIZE);
            REQUIRE(epoch_operation_queue_enqueue(epoch_operation_queue) == nullptr);
        }

        epoch_operation_queue_free(epoch_operation_queue);
    }

    SECTION("epoch_operation_queue_mark_completed") {
        epoch_operation_queue_t *epoch_operation_queue = epoch_operation_queue_init();

        SECTION("mark one") {
            epoch_operation_queue_operation_t *op = epoch_operation_queue_enqueue(epoch_operation_queue);

            epoch_operation_queue_mark_completed(op);

            REQUIRE(op->data.completed == true);
            REQUIRE(ring_bounded_queue_spsc_uint64_get_length(epoch_operation_queue->ring) == 1);
        }

        SECTION("mark two") {
            epoch_operation_queue_operation_t *op1 = epoch_operation_queue_enqueue(epoch_operation_queue);
            epoch_operation_queue_operation_t *op2 = epoch_operation_queue_enqueue(epoch_operation_queue);

            epoch_operation_queue_mark_completed(op2);
            epoch_operation_queue_mark_completed(op1);

            REQUIRE(op2->data.completed == true);
            REQUIRE(ring_bounded_queue_spsc_uint64_get_length(epoch_operation_queue->ring) == 2);
        }

        epoch_operation_queue_free(epoch_operation_queue);
    }

    SECTION("epoch_operation_queue_get_latest_epoch") {
        epoch_operation_queue_t *epoch_operation_queue = epoch_operation_queue_init();

        SECTION("empty list") {
            REQUIRE(epoch_operation_queue_get_latest_epoch(epoch_operation_queue) == 0);
        }

        SECTION("enqueue and mark operation completed one") {
            epoch_operation_queue_operation_t *op = epoch_operation_queue_enqueue(epoch_operation_queue);
            epoch_operation_queue_mark_completed(op);

            REQUIRE(epoch_operation_queue_get_latest_epoch(epoch_operation_queue) == op->data.start_epoch);
        }

        SECTION("enqueue and mark operation completed two in reverse order one at time") {
            epoch_operation_queue_operation_t *op1 = epoch_operation_queue_enqueue(epoch_operation_queue);
            epoch_operation_queue_operation_t *op2 = epoch_operation_queue_enqueue(epoch_operation_queue);

            epoch_operation_queue_mark_completed(op1);
            REQUIRE(epoch_operation_queue_get_latest_epoch(epoch_operation_queue) == op1->data.start_epoch);

            epoch_operation_queue_mark_completed(op2);
            REQUIRE(epoch_operation_queue_get_latest_epoch(epoch_operation_queue) == op2->data.start_epoch);
        }

        SECTION("enqueue and mark operation completed two in reverse order") {
            epoch_operation_queue_operation_t *op1 = epoch_operation_queue_enqueue(epoch_operation_queue);
            epoch_operation_queue_operation_t *op2 = epoch_operation_queue_enqueue(epoch_operation_queue);
            epoch_operation_queue_mark_completed(op2);
            epoch_operation_queue_mark_completed(op1);

            REQUIRE(epoch_operation_queue_get_latest_epoch(epoch_operation_queue) == op2->data.start_epoch);
        }

        SECTION("enqueue two but mark operation completed only one") {
            epoch_operation_queue_enqueue(epoch_operation_queue);
            epoch_operation_queue_operation_t *op2 = epoch_operation_queue_enqueue(epoch_operation_queue);
            epoch_operation_queue_mark_completed(op2);

            REQUIRE(epoch_operation_queue_get_latest_epoch(epoch_operation_queue) == 0);
        }

        epoch_operation_queue_free(epoch_operation_queue);
    }
}
