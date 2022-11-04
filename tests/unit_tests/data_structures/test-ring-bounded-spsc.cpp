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
        REQUIRE(rb->maxsize == 10);
        REQUIRE(rb->head == 0);
        REQUIRE(rb->tail == -1);
        REQUIRE(rb->count == 0);
        REQUIRE(rb->items != NULL);

        ring_bounded_spsc_free(rb);
    }

    SECTION("ring_bounded_spsc_count") {
        ring_bounded_spsc_t* rb = ring_bounded_spsc_init(10);

        rb->count = 5;
        REQUIRE(ring_bounded_spsc_count(rb) == 5);

        ring_bounded_spsc_free(rb);
    }

    SECTION("ring_bounded_spsc_is_empty") {
        ring_bounded_spsc_t* rb = ring_bounded_spsc_init(10);

        SECTION("empty") {
            rb->count = 0;
            REQUIRE(ring_bounded_spsc_is_empty(rb) == true);
        }

        SECTION("not empty") {
            rb->count = 5;
            REQUIRE(ring_bounded_spsc_is_empty(rb) == false);
        }

        ring_bounded_spsc_free(rb);
    }

    SECTION("ring_bounded_spsc_is_full") {
        ring_bounded_spsc_t* rb = ring_bounded_spsc_init(10);

        SECTION("full") {
            rb->count = rb->maxsize;
            REQUIRE(ring_bounded_spsc_is_full(rb) == true);
        }

        SECTION("not full") {
            rb->count = 0;
            REQUIRE(ring_bounded_spsc_is_full(rb) == false);
        }

        ring_bounded_spsc_free(rb);
    }

    SECTION("ring_bounded_spsc_enqueue") {
        bool res;
        ring_bounded_spsc_t* rb = ring_bounded_spsc_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * rb->maxsize);

        SECTION("enqueue 1") {
            res = ring_bounded_spsc_enqueue(rb, random_values_from_memory[0]);

            REQUIRE(res == true);
            REQUIRE(rb->count == 1);
            REQUIRE(rb->head == 0);
            REQUIRE(rb->tail == 0);
            REQUIRE(rb->items[0] == random_values_from_memory[0]);
        }

        SECTION("enqueue 2") {
            res = ring_bounded_spsc_enqueue(rb, random_values_from_memory[0]);
            REQUIRE(res == true);

            res = ring_bounded_spsc_enqueue(rb, random_values_from_memory[1]);
            REQUIRE(res == true);

            REQUIRE(rb->count == 2);
            REQUIRE(rb->head == 0);
            REQUIRE(rb->tail == 1);
            REQUIRE(rb->items[1] == random_values_from_memory[1]);
        }

        SECTION("fill circular queue") {
            for(int i = 0; i < rb->maxsize; i++) {
                res = ring_bounded_spsc_enqueue(rb, random_values_from_memory[i]);
                REQUIRE(res == true);
            }

            REQUIRE(rb->count == rb->maxsize);
            REQUIRE(rb->head == 0);
            REQUIRE(rb->tail == rb->maxsize - 1);
        }

        SECTION("overflow circular queue") {
            for(int i = 0; i < rb->maxsize; i++) {
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
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * rb->maxsize);

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
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * rb->maxsize);

        SECTION("dequeue 1") {
            ring_bounded_spsc_enqueue(rb, random_values_from_memory[0]);

            void* value = ring_bounded_spsc_dequeue(rb);

            REQUIRE(value == random_values_from_memory[0]);
            REQUIRE(rb->count == 0);
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
            REQUIRE(rb->count == 0);
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
            REQUIRE(rb->count == 0);
            REQUIRE(rb->head == 2);
            REQUIRE(rb->tail == 1);
        }

        SECTION("fill and empty circular queue") {
            for(int i = 0; i < rb->maxsize; i++) {
                ring_bounded_spsc_enqueue(rb, random_values_from_memory[i]);
            }

            for(int i = 0; i < rb->maxsize; i++) {
                void* value = ring_bounded_spsc_dequeue(rb);
                REQUIRE(value == random_values_from_memory[i]);
            }

            REQUIRE(rb->count == 0);
            REQUIRE(rb->head == 0);
            REQUIRE(rb->tail == rb->maxsize - 1);
        }

        ring_bounded_spsc_free(rb);
        free(random_values_from_memory);
    }
}
