/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include "data_structures/ring_bounded_spsc/ring_bounded_spsc.h"

TEST_CASE("data_structures/ring_bounded_spsc/ring_bounded_spsc.c", "[data_structures][ring_bounded_spsc]") {
    SECTION("ring_bounded_spsc_init") {
        ring_bounded_spsc_t* rb = ring_bounded_spsc_init(10);

        REQUIRE(rb != NULL);
        REQUIRE(rb->size == 10);
        REQUIRE(rb->head == 0);
        REQUIRE(rb->tail == -1);
        REQUIRE(rb->length == 0);
        REQUIRE(rb->items != NULL);

        ring_bounded_spsc_free(rb);
    }

    SECTION("ring_bounded_spsc_get_length") {
        ring_bounded_spsc_t* rb = ring_bounded_spsc_init(10);

        rb->length = 5;
        REQUIRE(ring_bounded_spsc_get_length(rb) == 5);

        ring_bounded_spsc_free(rb);
    }

    SECTION("ring_bounded_spsc_is_empty") {
        ring_bounded_spsc_t* rb = ring_bounded_spsc_init(10);

        SECTION("empty") {
            rb->length = 0;
            REQUIRE(ring_bounded_spsc_is_empty(rb) == true);
        }

        SECTION("not empty") {
            rb->length = 5;
            REQUIRE(ring_bounded_spsc_is_empty(rb) == false);
        }

        ring_bounded_spsc_free(rb);
    }

    SECTION("ring_bounded_spsc_is_full") {
        ring_bounded_spsc_t* rb = ring_bounded_spsc_init(10);

        SECTION("full") {
            rb->length = rb->size;
            REQUIRE(ring_bounded_spsc_is_full(rb) == true);
        }

        SECTION("not full") {
            rb->length = 0;
            REQUIRE(ring_bounded_spsc_is_full(rb) == false);
        }

        ring_bounded_spsc_free(rb);
    }

    SECTION("ring_bounded_spsc_enqueue") {
        bool res;
        ring_bounded_spsc_t* rb = ring_bounded_spsc_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * rb->size);

        SECTION("enqueue 1") {
            res = ring_bounded_spsc_enqueue(rb, random_values_from_memory[0]);

            REQUIRE(res == true);
            REQUIRE(rb->length == 1);
            REQUIRE(rb->head == 0);
            REQUIRE(rb->tail == 0);
            REQUIRE(rb->items[0] == random_values_from_memory[0]);
        }

        SECTION("enqueue 2") {
            res = ring_bounded_spsc_enqueue(rb, random_values_from_memory[0]);
            REQUIRE(res == true);

            res = ring_bounded_spsc_enqueue(rb, random_values_from_memory[1]);
            REQUIRE(res == true);

            REQUIRE(rb->length == 2);
            REQUIRE(rb->head == 0);
            REQUIRE(rb->tail == 1);
            REQUIRE(rb->items[1] == random_values_from_memory[1]);
        }

        SECTION("fill circular queue") {
            for(int i = 0; i < rb->size; i++) {
                res = ring_bounded_spsc_enqueue(rb, random_values_from_memory[i]);
                REQUIRE(res == true);
            }

            REQUIRE(rb->length == rb->size);
            REQUIRE(rb->head == 0);
            REQUIRE(rb->tail == rb->size - 1);
        }

        SECTION("overflow circular queue") {
            for(int i = 0; i < rb->size; i++) {
                res = ring_bounded_spsc_enqueue(rb, random_values_from_memory[i]);
                REQUIRE(res == true);
            }

            res = ring_bounded_spsc_enqueue(rb, random_values_from_memory[0]);
            REQUIRE(res == false);
        }

        ring_bounded_spsc_free(rb);
        free(random_values_from_memory);
    }

    SECTION("ring_bounded_spsc_peek") {
        ring_bounded_spsc_t* rb = ring_bounded_spsc_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * rb->size);

        SECTION("enqueue 1") {
            ring_bounded_spsc_enqueue(rb, random_values_from_memory[0]);

            REQUIRE(ring_bounded_spsc_peek(rb) == random_values_from_memory[0]);
        }

        SECTION("enqueue 2") {
            ring_bounded_spsc_enqueue(rb, random_values_from_memory[0]);
            ring_bounded_spsc_enqueue(rb, random_values_from_memory[1]);

            REQUIRE(ring_bounded_spsc_peek(rb) == random_values_from_memory[0]);
        }

        ring_bounded_spsc_free(rb);
        free(random_values_from_memory);
    }

    SECTION("ring_bounded_spsc_dequeue") {
        ring_bounded_spsc_t* rb = ring_bounded_spsc_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * rb->size);

        SECTION("dequeue 1") {
            ring_bounded_spsc_enqueue(rb, random_values_from_memory[0]);

            void* value = ring_bounded_spsc_dequeue(rb);

            REQUIRE(value == random_values_from_memory[0]);
            REQUIRE(rb->length == 0);
            REQUIRE(rb->head == 1);
            REQUIRE(rb->tail == 0);
        }

        SECTION("dequeue 2") {
            ring_bounded_spsc_enqueue(rb, random_values_from_memory[0]);
            ring_bounded_spsc_enqueue(rb, random_values_from_memory[1]);

            void* value1 = ring_bounded_spsc_dequeue(rb);
            void* value2 = ring_bounded_spsc_dequeue(rb);

            REQUIRE(value1 == random_values_from_memory[0]);
            REQUIRE(value2 == random_values_from_memory[1]);
            REQUIRE(rb->length == 0);
            REQUIRE(rb->head == 2);
            REQUIRE(rb->tail == 1);
        }

        SECTION("enqueue and dequeue twice 2") {
            ring_bounded_spsc_enqueue(rb, random_values_from_memory[0]);
            void* value1 = ring_bounded_spsc_dequeue(rb);

            ring_bounded_spsc_enqueue(rb, random_values_from_memory[1]);
            void* value2 = ring_bounded_spsc_dequeue(rb);

            REQUIRE(value1 == random_values_from_memory[0]);
            REQUIRE(value2 == random_values_from_memory[1]);
            REQUIRE(rb->length == 0);
            REQUIRE(rb->head == 2);
            REQUIRE(rb->tail == 1);
        }

        SECTION("fill and empty circular queue") {
            for(int i = 0; i < rb->size; i++) {
                ring_bounded_spsc_enqueue(rb, random_values_from_memory[i]);
            }

            for(int i = 0; i < rb->size; i++) {
                void* value = ring_bounded_spsc_dequeue(rb);
                REQUIRE(value == random_values_from_memory[i]);
            }

            REQUIRE(rb->length == 0);
            REQUIRE(rb->head == 0);
            REQUIRE(rb->tail == rb->size - 1);
        }

        ring_bounded_spsc_free(rb);
        free(random_values_from_memory);
    }
}
