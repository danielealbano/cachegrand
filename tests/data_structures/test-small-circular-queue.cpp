#include <catch2/catch.hpp>

#include "data_structures/small_circular_queue/small_circular_queue.h"

TEST_CASE("data_structures/circular_queue/small_circular_queue.c", "[data_structures][small_circular_queue]") {
    SECTION("small_circular_queue_init") {
        small_circular_queue_t* scq = small_circular_queue_init(10);

        REQUIRE(scq != NULL);
        REQUIRE(scq->maxsize == 10);
        REQUIRE(scq->head == 0);
        REQUIRE(scq->tail == -1);
        REQUIRE(scq->count == 0);
        REQUIRE(scq->items != NULL);

        small_circular_queue_free(scq);
    }

    SECTION("small_circular_queue_count") {
        small_circular_queue_t* scq = small_circular_queue_init(10);

        scq->count = 5;
        REQUIRE(small_circular_queue_count(scq) == 5);

        small_circular_queue_free(scq);
    }

    SECTION("small_circular_queue_is_empty") {
        small_circular_queue_t* scq = small_circular_queue_init(10);

        SECTION("empty") {
            scq->count = 0;
            REQUIRE(small_circular_queue_is_empty(scq) == true);
        }

        SECTION("not empty") {
            scq->count = 5;
            REQUIRE(small_circular_queue_is_empty(scq) == false);
        }

        small_circular_queue_free(scq);
    }

    SECTION("small_circular_queue_is_full") {
        small_circular_queue_t* scq = small_circular_queue_init(10);

        SECTION("full") {
            scq->count = scq->maxsize;
            REQUIRE(small_circular_queue_is_full(scq) == true);
        }

        SECTION("not full") {
            scq->count = 0;
            REQUIRE(small_circular_queue_is_full(scq) == false);
        }

        small_circular_queue_free(scq);
    }

    SECTION("small_circular_queue_enqueue") {
        bool res;
        small_circular_queue_t* scq = small_circular_queue_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * scq->maxsize);

        SECTION("enqueue 1") {
            res = small_circular_queue_enqueue(scq, random_values_from_memory[0]);

            REQUIRE(res == true);
            REQUIRE(scq->count == 1);
            REQUIRE(scq->head == 0);
            REQUIRE(scq->tail == 0);
            REQUIRE(scq->items[0] == random_values_from_memory[0]);
        }

        SECTION("enqueue 2") {
            res = small_circular_queue_enqueue(scq, random_values_from_memory[0]);
            REQUIRE(res == true);

            res = small_circular_queue_enqueue(scq, random_values_from_memory[1]);
            REQUIRE(res == true);

            REQUIRE(scq->count == 2);
            REQUIRE(scq->head == 0);
            REQUIRE(scq->tail == 1);
            REQUIRE(scq->items[1] == random_values_from_memory[1]);
        }

        SECTION("fill circular queue") {
            for(int i = 0; i < scq->maxsize; i++) {
                res = small_circular_queue_enqueue(scq, random_values_from_memory[i]);
                REQUIRE(res == true);
            }

            REQUIRE(scq->count == scq->maxsize);
            REQUIRE(scq->head == 0);
            REQUIRE(scq->tail == scq->maxsize - 1);
        }

        SECTION("overflow circular queue") {
            for(int i = 0; i < scq->maxsize; i++) {
                res = small_circular_queue_enqueue(scq, random_values_from_memory[i]);
                REQUIRE(res == true);
            }

            res = small_circular_queue_enqueue(scq, random_values_from_memory[0]);
            REQUIRE(res == false);
        }

        small_circular_queue_free(scq);
        free(random_values_from_memory);
    }

    SECTION("small_circular_queue_peek") {
        small_circular_queue_t* scq = small_circular_queue_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * scq->maxsize);

        SECTION("enqueue 1") {
            small_circular_queue_enqueue(scq, random_values_from_memory[0]);

            REQUIRE(small_circular_queue_peek(scq) == random_values_from_memory[0]);
        }

        SECTION("enqueue 2") {
            small_circular_queue_enqueue(scq, random_values_from_memory[0]);
            small_circular_queue_enqueue(scq, random_values_from_memory[1]);

            REQUIRE(small_circular_queue_peek(scq) == random_values_from_memory[0]);
        }

        small_circular_queue_free(scq);
        free(random_values_from_memory);
    }

    SECTION("small_circular_queue_dequeue") {
        small_circular_queue_t* scq = small_circular_queue_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * scq->maxsize);

        SECTION("dequeue 1") {
            small_circular_queue_enqueue(scq, random_values_from_memory[0]);

            void* value = small_circular_queue_dequeue(scq);

            REQUIRE(value == random_values_from_memory[0]);
            REQUIRE(scq->count == 0);
            REQUIRE(scq->head == 1);
            REQUIRE(scq->tail == 0);
        }

        SECTION("dequeue 2") {
            small_circular_queue_enqueue(scq, random_values_from_memory[0]);
            small_circular_queue_enqueue(scq, random_values_from_memory[1]);

            void* value1 = small_circular_queue_dequeue(scq);
            void* value2 = small_circular_queue_dequeue(scq);

            REQUIRE(value1 == random_values_from_memory[0]);
            REQUIRE(value2 == random_values_from_memory[1]);
            REQUIRE(scq->count == 0);
            REQUIRE(scq->head == 2);
            REQUIRE(scq->tail == 1);
        }

        SECTION("enqueue and dequeue twice 2") {
            small_circular_queue_enqueue(scq, random_values_from_memory[0]);
            void* value1 = small_circular_queue_dequeue(scq);

            small_circular_queue_enqueue(scq, random_values_from_memory[1]);
            void* value2 = small_circular_queue_dequeue(scq);

            REQUIRE(value1 == random_values_from_memory[0]);
            REQUIRE(value2 == random_values_from_memory[1]);
            REQUIRE(scq->count == 0);
            REQUIRE(scq->head == 2);
            REQUIRE(scq->tail == 1);
        }

        SECTION("fill and empty circular queue") {
            for(int i = 0; i < scq->maxsize; i++) {
                small_circular_queue_enqueue(scq, random_values_from_memory[i]);
            }

            for(int i = 0; i < scq->maxsize; i++) {
                void* value = small_circular_queue_dequeue(scq);
                REQUIRE(value == random_values_from_memory[i]);
            }

            REQUIRE(scq->count == 0);
            REQUIRE(scq->head == 0);
            REQUIRE(scq->tail == scq->maxsize - 1);
        }

        small_circular_queue_free(scq);
        free(random_values_from_memory);
    }
}
