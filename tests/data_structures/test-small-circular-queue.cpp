#include <catch2/catch.hpp>

#include "data_structures/small_circular_queue/small_circular_queue.h"

TEST_CASE("data_structures/circular_queue/small_circular_queue.c", "[data_structures][small_circular_queue]") {
    SECTION("small_circular_queue_init") {
        small_circular_queue_t* cq = small_circular_queue_init(10);

        REQUIRE(cq != NULL);
        REQUIRE(cq->maxsize == 10);
        REQUIRE(cq->head == 0);
        REQUIRE(cq->tail == -1);
        REQUIRE(cq->count == 0);
        REQUIRE(cq->items != NULL);

        small_circular_queue_free(cq);
    }

    SECTION("small_circular_queue_count") {
        small_circular_queue_t* cq = small_circular_queue_init(10);

        cq->count = 5;
        REQUIRE(small_circular_queue_count(cq) == 5);

        small_circular_queue_free(cq);
    }

    SECTION("small_circular_queue_is_empty") {
        small_circular_queue_t* cq = small_circular_queue_init(10);

        SECTION("empty") {
            cq->count = 0;
            REQUIRE(small_circular_queue_is_empty(cq) == true);
        }

        SECTION("not empty") {
            cq->count = 5;
            REQUIRE(small_circular_queue_is_empty(cq) == false);
        }

        small_circular_queue_free(cq);
    }

    SECTION("small_circular_queue_is_full") {
        small_circular_queue_t* cq = small_circular_queue_init(10);

        SECTION("full") {
            cq->count = cq->maxsize;
            REQUIRE(small_circular_queue_is_full(cq) == true);
        }

        SECTION("not full") {
            cq->count = 0;
            REQUIRE(small_circular_queue_is_full(cq) == false);
        }

        small_circular_queue_free(cq);
    }

    SECTION("small_circular_queue_enqueue") {
        bool res;
        small_circular_queue_t* cq = small_circular_queue_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * cq->maxsize);

        SECTION("enqueue 1") {
            res = small_circular_queue_enqueue(cq, random_values_from_memory[0]);

            REQUIRE(res == true);
            REQUIRE(cq->count == 1);
            REQUIRE(cq->head == 0);
            REQUIRE(cq->tail == 0);
            REQUIRE(cq->items[0] == random_values_from_memory[0]);
        }

        SECTION("enqueue 2") {
            res = small_circular_queue_enqueue(cq, random_values_from_memory[0]);
            REQUIRE(res == true);

            res = small_circular_queue_enqueue(cq, random_values_from_memory[1]);
            REQUIRE(res == true);

            REQUIRE(cq->count == 2);
            REQUIRE(cq->head == 0);
            REQUIRE(cq->tail == 1);
            REQUIRE(cq->items[1] == random_values_from_memory[1]);
        }

        SECTION("fill circular queue") {
            for(int i = 0; i < cq->maxsize; i++) {
                res = small_circular_queue_enqueue(cq, random_values_from_memory[i]);
                REQUIRE(res == true);
            }

            REQUIRE(cq->count == cq->maxsize);
            REQUIRE(cq->head == 0);
            REQUIRE(cq->tail == cq->maxsize - 1);
        }

        SECTION("overflow circular queue") {
            for(int i = 0; i < cq->maxsize; i++) {
                res = small_circular_queue_enqueue(cq, random_values_from_memory[i]);
                REQUIRE(res == true);
            }

            res = small_circular_queue_enqueue(cq, random_values_from_memory[0]);
            REQUIRE(res == false);
        }

        small_circular_queue_free(cq);
        free(random_values_from_memory);
    }

    SECTION("small_circular_queue_peek") {
        small_circular_queue_t* cq = small_circular_queue_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * cq->maxsize);

        SECTION("enqueue 1") {
            small_circular_queue_enqueue(cq, random_values_from_memory[0]);

            REQUIRE(small_circular_queue_peek(cq) == random_values_from_memory[0]);
        }

        SECTION("enqueue 2") {
            small_circular_queue_enqueue(cq, random_values_from_memory[0]);
            small_circular_queue_enqueue(cq, random_values_from_memory[1]);

            REQUIRE(small_circular_queue_peek(cq) == random_values_from_memory[0]);
        }

        small_circular_queue_free(cq);
        free(random_values_from_memory);
    }

    SECTION("small_circular_queue_dequeue") {
        small_circular_queue_t* cq = small_circular_queue_init(10);
        void** random_values_from_memory = (void**)malloc(sizeof(void*) * cq->maxsize);

        SECTION("dequeue 1") {
            small_circular_queue_enqueue(cq, random_values_from_memory[0]);

            void* value = small_circular_queue_dequeue(cq);

            REQUIRE(value == random_values_from_memory[0]);
            REQUIRE(cq->count == 0);
            REQUIRE(cq->head == 1);
            REQUIRE(cq->tail == 0);
        }

        SECTION("dequeue 2") {
            small_circular_queue_enqueue(cq, random_values_from_memory[0]);
            small_circular_queue_enqueue(cq, random_values_from_memory[1]);

            void* value1 = small_circular_queue_dequeue(cq);
            void* value2 = small_circular_queue_dequeue(cq);

            REQUIRE(value1 == random_values_from_memory[0]);
            REQUIRE(value2 == random_values_from_memory[1]);
            REQUIRE(cq->count == 0);
            REQUIRE(cq->head == 2);
            REQUIRE(cq->tail == 1);
        }

        SECTION("enqueue and dequeue twice 2") {
            small_circular_queue_enqueue(cq, random_values_from_memory[0]);
            void* value1 = small_circular_queue_dequeue(cq);

            small_circular_queue_enqueue(cq, random_values_from_memory[1]);
            void* value2 = small_circular_queue_dequeue(cq);

            REQUIRE(value1 == random_values_from_memory[0]);
            REQUIRE(value2 == random_values_from_memory[1]);
            REQUIRE(cq->count == 0);
            REQUIRE(cq->head == 2);
            REQUIRE(cq->tail == 1);
        }

        SECTION("fill and empty circular queue") {
            for(int i = 0; i < cq->maxsize; i++) {
                small_circular_queue_enqueue(cq, random_values_from_memory[i]);
            }

            for(int i = 0; i < cq->maxsize; i++) {
                void* value = small_circular_queue_dequeue(cq);
                REQUIRE(value == random_values_from_memory[i]);
            }

            REQUIRE(cq->count == 0);
            REQUIRE(cq->head == 0);
            REQUIRE(cq->tail == cq->maxsize - 1);
        }

        small_circular_queue_free(cq);
        free(random_values_from_memory);
    }
}
