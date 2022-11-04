/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include "exttypes.h"
#include "data_structures/ring_bounded_mpmc/ring_bounded_mpmc.h"

TEST_CASE("data_structures/ring_bounded_mpmc/ring_bounded_mpmc.c", "[data_structures][ring_bounded_mpmc]") {
    SECTION("ring_bounded_mpmc_init") {
        ring_bounded_mpmc_t* rb = ring_bounded_mpmc_init(10);

        REQUIRE(rb != NULL);
        REQUIRE(rb->header.data.maxsize == 10);
        REQUIRE(rb->header.data.head == 0);
        REQUIRE(rb->header.data.tail == -1);
        REQUIRE(rb->header.data.count == 0);
        REQUIRE(rb->items != NULL);

        ring_bounded_mpmc_free(rb);
    }

    SECTION("ring_bounded_mpmc_count") {
        ring_bounded_mpmc_t* rb = ring_bounded_mpmc_init(10);

        rb->header.data.count = 5;
        REQUIRE(ring_bounded_mpmc_count(rb) == 5);

        ring_bounded_mpmc_free(rb);
    }

    SECTION("ring_bounded_mpmc_is_empty") {
        ring_bounded_mpmc_t* rb = ring_bounded_mpmc_init(10);

        SECTION("empty") {
            rb->header.data.count = 0;
            REQUIRE(ring_bounded_mpmc_is_empty(rb) == true);
        }

        SECTION("not empty") {
            rb->header.data.count = 5;
            REQUIRE(ring_bounded_mpmc_is_empty(rb) == false);
        }

        ring_bounded_mpmc_free(rb);
    }

    SECTION("ring_bounded_mpmc_is_full") {
        ring_bounded_mpmc_t* rb = ring_bounded_mpmc_init(10);

        SECTION("full") {
            rb->header.data.count = rb->header.data.maxsize;
            REQUIRE(ring_bounded_mpmc_is_full(rb) == true);
        }

        SECTION("not full") {
            rb->header.data.count = 0;
            REQUIRE(ring_bounded_mpmc_is_full(rb) == false);
        }

        ring_bounded_mpmc_free(rb);
    }

    SECTION("ring_bounded_mpmc_enqueue") {
        bool res;
        ring_bounded_mpmc_t* rb = ring_bounded_mpmc_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * rb->header.data.maxsize);

        SECTION("enqueue 1") {
            res = ring_bounded_mpmc_enqueue(rb, random_values_from_memory[0]);

            REQUIRE(res == true);
            REQUIRE(rb->header.data.count == 1);
            REQUIRE(rb->header.data.head == 0);
            REQUIRE(rb->header.data.tail == 0);
            REQUIRE(rb->items[0] == random_values_from_memory[0]);
        }

        SECTION("enqueue 2") {
            res = ring_bounded_mpmc_enqueue(rb, random_values_from_memory[0]);
            REQUIRE(res == true);

            res = ring_bounded_mpmc_enqueue(rb, random_values_from_memory[1]);
            REQUIRE(res == true);

            REQUIRE(rb->header.data.count == 2);
            REQUIRE(rb->header.data.head == 0);
            REQUIRE(rb->header.data.tail == 1);
            REQUIRE(rb->items[1] == random_values_from_memory[1]);
        }

        SECTION("fill circular queue") {
            for(int i = 0; i < rb->header.data.maxsize; i++) {
                res = ring_bounded_mpmc_enqueue(rb, random_values_from_memory[i]);
                REQUIRE(res == true);
            }

            REQUIRE(rb->header.data.count == rb->header.data.maxsize);
            REQUIRE(rb->header.data.head == 0);
            REQUIRE(rb->header.data.tail == rb->header.data.maxsize - 1);
        }

        SECTION("overflow circular queue") {
            for(int i = 0; i < rb->header.data.maxsize; i++) {
                res = ring_bounded_mpmc_enqueue(rb, random_values_from_memory[i]);
                REQUIRE(res == true);
            }

            res = ring_bounded_mpmc_enqueue(rb, random_values_from_memory[0]);
            REQUIRE(res == false);
        }

        ring_bounded_mpmc_free(rb);
        free(random_values_from_memory);
    }

    SECTION("ring_bounded_mpmc_peek") {
        ring_bounded_mpmc_t* rb = ring_bounded_mpmc_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * rb->header.data.maxsize);

        SECTION("enqueue 1") {
            ring_bounded_mpmc_enqueue(rb, random_values_from_memory[0]);

            REQUIRE(ring_bounded_mpmc_peek(rb) == random_values_from_memory[0]);
        }

        SECTION("enqueue 2") {
            ring_bounded_mpmc_enqueue(rb, random_values_from_memory[0]);
            ring_bounded_mpmc_enqueue(rb, random_values_from_memory[1]);

            REQUIRE(ring_bounded_mpmc_peek(rb) == random_values_from_memory[0]);
        }

        ring_bounded_mpmc_free(rb);
        free(random_values_from_memory);
    }

    SECTION("ring_bounded_mpmc_dequeue") {
        ring_bounded_mpmc_t* rb = ring_bounded_mpmc_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * rb->header.data.maxsize);

        SECTION("dequeue 1") {
            ring_bounded_mpmc_enqueue(rb, random_values_from_memory[0]);

            void* value = ring_bounded_mpmc_dequeue(rb);

            REQUIRE(value == random_values_from_memory[0]);
            REQUIRE(rb->header.data.count == 0);
            REQUIRE(rb->header.data.head == 1);
            REQUIRE(rb->header.data.tail == 0);
        }

        SECTION("dequeue 2") {
            ring_bounded_mpmc_enqueue(rb, random_values_from_memory[0]);
            ring_bounded_mpmc_enqueue(rb, random_values_from_memory[1]);

            void* value1 = ring_bounded_mpmc_dequeue(rb);
            void* value2 = ring_bounded_mpmc_dequeue(rb);

            REQUIRE(value1 == random_values_from_memory[0]);
            REQUIRE(value2 == random_values_from_memory[1]);
            REQUIRE(rb->header.data.count == 0);
            REQUIRE(rb->header.data.head == 2);
            REQUIRE(rb->header.data.tail == 1);
        }

        SECTION("enqueue and dequeue twice 2") {
            ring_bounded_mpmc_enqueue(rb, random_values_from_memory[0]);
            void* value1 = ring_bounded_mpmc_dequeue(rb);

            ring_bounded_mpmc_enqueue(rb, random_values_from_memory[1]);
            void* value2 = ring_bounded_mpmc_dequeue(rb);

            REQUIRE(value1 == random_values_from_memory[0]);
            REQUIRE(value2 == random_values_from_memory[1]);
            REQUIRE(rb->header.data.count == 0);
            REQUIRE(rb->header.data.head == 2);
            REQUIRE(rb->header.data.tail == 1);
        }

        SECTION("fill and empty circular queue") {
            for(int i = 0; i < rb->header.data.maxsize; i++) {
                ring_bounded_mpmc_enqueue(rb, random_values_from_memory[i]);
            }

            for(int i = 0; i < rb->header.data.maxsize; i++) {
                void* value = ring_bounded_mpmc_dequeue(rb);
                REQUIRE(value == random_values_from_memory[i]);
            }

            REQUIRE(rb->header.data.count == 0);
            REQUIRE(rb->header.data.head == 0);
            REQUIRE(rb->header.data.tail == rb->header.data.maxsize - 1);
        }

        ring_bounded_mpmc_free(rb);
        free(random_values_from_memory);
    }
}
