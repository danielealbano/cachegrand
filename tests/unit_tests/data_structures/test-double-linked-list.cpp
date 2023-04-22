/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include "data_structures/double_linked_list/double_linked_list.h"

#define TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(LENGTH, ...) { \
        int items_length = LENGTH; \
        double_linked_list_item_t* items[LENGTH]; \
        double_linked_list_t* list = double_linked_list_init(); \
        for(int i = 0; i < items_length; i++) { \
            double_linked_list_item_t* item = double_linked_list_item_init(); \
            double_linked_list_insert_item_after(list, item, list->tail); \
            items[i] = item; \
        } \
        \
        __VA_ARGS__ \
        \
        for(int i = 0; i < items_length; i++) { \
            double_linked_list_item_free(items[i]); \
        } \
        double_linked_list_free(list); \
    }

TEST_CASE("data_structures/double_linked_list/double_linked_list.c", "[data_structures][double_linked_list]") {
    SECTION("double_linked_list_item_init") {
        double_linked_list_item_t* item = double_linked_list_item_init();

        REQUIRE(item != NULL);
        REQUIRE(item->prev == NULL);
        REQUIRE(item->next == NULL);

        double_linked_list_item_free(item);
    }

    SECTION("double_linked_list_init") {
        double_linked_list_t* list = double_linked_list_init();

        REQUIRE(list != NULL);
        REQUIRE(list->count == 0);
        REQUIRE(list->head == NULL);
        REQUIRE(list->tail == NULL);

        double_linked_list_free(list);
    }

    SECTION("double_linked_list_insert_item_before") {
        SECTION("insert one double_linked_list_item before head") {
            double_linked_list_t* list = double_linked_list_init();
            double_linked_list_item_t* item1 = double_linked_list_item_init();

            double_linked_list_insert_item_before(list, item1, list->head);

            REQUIRE(list->head == item1);
            REQUIRE(list->tail == item1);
            REQUIRE(list->count == 1);
            REQUIRE(item1->prev == NULL);
            REQUIRE(item1->next == NULL);

            double_linked_list_item_free(item1);
            double_linked_list_free(list);
        }

        SECTION("insert two items before head") {
            double_linked_list_t* list = double_linked_list_init();
            double_linked_list_item_t* item1 = double_linked_list_item_init();
            double_linked_list_item_t* item2 = double_linked_list_item_init();

            double_linked_list_insert_item_before(list, item1, list->head);
            double_linked_list_insert_item_before(list, item2, list->head);

            REQUIRE(list->head == item2);
            REQUIRE(list->tail == item1);
            REQUIRE(list->count == 2);
            REQUIRE(item2->prev == NULL);
            REQUIRE(item2->next == item1);
            REQUIRE(item1->prev == item2);
            REQUIRE(item1->next == NULL);

            double_linked_list_item_free(item1);
            double_linked_list_item_free(item2);
            double_linked_list_free(list);
        }

        SECTION("insert three items before head") {
            double_linked_list_t* list = double_linked_list_init();
            double_linked_list_item_t* item1 = double_linked_list_item_init();
            double_linked_list_item_t* item2 = double_linked_list_item_init();
            double_linked_list_item_t* item3 = double_linked_list_item_init();

            double_linked_list_insert_item_before(list, item1, list->head);
            double_linked_list_insert_item_before(list, item2, list->head);
            double_linked_list_insert_item_before(list, item3, list->head);

            REQUIRE(list->head == item3);
            REQUIRE(list->tail == item1);
            REQUIRE(list->count == 3);
            REQUIRE(item3->prev == NULL);
            REQUIRE(item3->next == item2);
            REQUIRE(item2->prev == item3);
            REQUIRE(item2->next == item1);
            REQUIRE(item1->prev == item2);
            REQUIRE(item1->next == NULL);

            double_linked_list_item_free(item1);
            double_linked_list_item_free(item2);
            double_linked_list_item_free(item3);
            double_linked_list_free(list);
        }

        SECTION("insert before double_linked_list_item") {
            double_linked_list_t* list = double_linked_list_init();
            double_linked_list_item_t* item1 = double_linked_list_item_init();
            double_linked_list_item_t* item2 = double_linked_list_item_init();
            double_linked_list_item_t* item3 = double_linked_list_item_init();
            double_linked_list_item_t* item4 = double_linked_list_item_init();

            double_linked_list_insert_item_before(list, item1, list->head);
            double_linked_list_insert_item_before(list, item2, list->head);
            double_linked_list_insert_item_before(list, item3, list->head);
            double_linked_list_insert_item_before(list, item4, item2);

            REQUIRE(list->head == item3);
            REQUIRE(list->tail == item1);
            REQUIRE(list->count == 4);
            REQUIRE(item3->prev == NULL);
            REQUIRE(item3->next == item4);
            REQUIRE(item4->prev == item3);
            REQUIRE(item4->next == item2);
            REQUIRE(item2->prev == item4);
            REQUIRE(item2->next == item1);
            REQUIRE(item1->prev == item2);
            REQUIRE(item1->next == NULL);

            double_linked_list_item_free(item1);
            double_linked_list_item_free(item2);
            double_linked_list_item_free(item3);
            double_linked_list_item_free(item4);
            double_linked_list_free(list);
        }
    }




    SECTION("double_linked_list_insert_item_after") {
        SECTION("insert one double_linked_list_item after tail") {
            double_linked_list_t* list = double_linked_list_init();
            double_linked_list_item_t* item1 = double_linked_list_item_init();

            double_linked_list_insert_item_after(list, item1, list->tail);

            REQUIRE(list->head == item1);
            REQUIRE(list->tail == item1);
            REQUIRE(list->count == 1);
            REQUIRE(item1->prev == NULL);
            REQUIRE(item1->next == NULL);

            double_linked_list_item_free(item1);
            double_linked_list_free(list);
        }

        SECTION("insert two items after tail") {
            double_linked_list_t* list = double_linked_list_init();
            double_linked_list_item_t* item1 = double_linked_list_item_init();
            double_linked_list_item_t* item2 = double_linked_list_item_init();

            double_linked_list_insert_item_after(list, item1, list->tail);
            double_linked_list_insert_item_after(list, item2, list->tail);

            REQUIRE(list->head == item1);
            REQUIRE(list->tail == item2);
            REQUIRE(list->count == 2);
            REQUIRE(item1->prev == NULL);
            REQUIRE(item1->next == item2);
            REQUIRE(item2->prev == item1);
            REQUIRE(item2->next == NULL);

            double_linked_list_item_free(item1);
            double_linked_list_item_free(item2);
            double_linked_list_free(list);
        }

        SECTION("insert three items after tail") {
            double_linked_list_t* list = double_linked_list_init();
            double_linked_list_item_t* item1 = double_linked_list_item_init();
            double_linked_list_item_t* item2 = double_linked_list_item_init();
            double_linked_list_item_t* item3 = double_linked_list_item_init();

            double_linked_list_insert_item_after(list, item1, list->tail);
            double_linked_list_insert_item_after(list, item2, list->tail);
            double_linked_list_insert_item_after(list, item3, list->tail);

            REQUIRE(list->head == item1);
            REQUIRE(list->tail == item3);
            REQUIRE(list->count == 3);
            REQUIRE(item1->prev == NULL);
            REQUIRE(item1->next == item2);
            REQUIRE(item2->prev == item1);
            REQUIRE(item2->next == item3);
            REQUIRE(item3->prev == item2);
            REQUIRE(item3->next == NULL);

            double_linked_list_item_free(item1);
            double_linked_list_item_free(item2);
            double_linked_list_item_free(item3);
            double_linked_list_free(list);
        }

        SECTION("insert after double_linked_list_item") {
            double_linked_list_t* list = double_linked_list_init();
            double_linked_list_item_t* item1 = double_linked_list_item_init();
            double_linked_list_item_t* item2 = double_linked_list_item_init();
            double_linked_list_item_t* item3 = double_linked_list_item_init();
            double_linked_list_item_t* item4 = double_linked_list_item_init();

            double_linked_list_insert_item_after(list, item1, list->tail);
            double_linked_list_insert_item_after(list, item2, list->tail);
            double_linked_list_insert_item_after(list, item3, list->tail);
            double_linked_list_insert_item_after(list, item4, item2);

            REQUIRE(list->head == item1);
            REQUIRE(list->tail == item3);
            REQUIRE(list->count == 4);
            REQUIRE(item1->prev == NULL);
            REQUIRE(item1->next == item2);
            REQUIRE(item2->prev == item1);
            REQUIRE(item2->next == item4);
            REQUIRE(item4->prev == item2);
            REQUIRE(item4->next == item3);
            REQUIRE(item3->prev == item4);
            REQUIRE(item3->next == NULL);

            double_linked_list_item_free(item1);
            double_linked_list_item_free(item2);
            double_linked_list_item_free(item3);
            double_linked_list_item_free(item4);
            double_linked_list_free(list);
        }
    }

    SECTION("double_linked_list_push_item") {
        SECTION("push one items") {
            double_linked_list_t* list = double_linked_list_init();
            double_linked_list_item_t* item1 = double_linked_list_item_init();

            double_linked_list_push_item(list, item1);

            REQUIRE(list->head == item1);
            REQUIRE(list->tail == item1);
            REQUIRE(list->count == 1);
            REQUIRE(item1->prev == NULL);
            REQUIRE(item1->next == NULL);

            double_linked_list_item_free(item1);
            double_linked_list_free(list);
        }

        SECTION("push two items") {
            double_linked_list_t* list = double_linked_list_init();
            double_linked_list_item_t* item1 = double_linked_list_item_init();
            double_linked_list_item_t* item2 = double_linked_list_item_init();

            double_linked_list_push_item(list, item1);
            double_linked_list_push_item(list, item2);

            REQUIRE(list->head == item1);
            REQUIRE(list->tail == item2);
            REQUIRE(list->count == 2);
            REQUIRE(item1->prev == NULL);
            REQUIRE(item1->next == item2);
            REQUIRE(item2->prev == item1);
            REQUIRE(item2->next == NULL);

            double_linked_list_item_free(item1);
            double_linked_list_item_free(item2);
            double_linked_list_free(list);
        }
    }

    SECTION("double_linked_list_unshift_item") {
        SECTION("unshift one items") {
            double_linked_list_t* list = double_linked_list_init();
            double_linked_list_item_t* item1 = double_linked_list_item_init();

            double_linked_list_unshift_item(list, item1);

            REQUIRE(list->head == item1);
            REQUIRE(list->tail == item1);
            REQUIRE(list->count == 1);
            REQUIRE(item1->prev == NULL);
            REQUIRE(item1->next == NULL);

            double_linked_list_item_free(item1);
            double_linked_list_free(list);
        }

        SECTION("unshift two items") {
            double_linked_list_t* list = double_linked_list_init();
            double_linked_list_item_t* item1 = double_linked_list_item_init();
            double_linked_list_item_t* item2 = double_linked_list_item_init();

            double_linked_list_unshift_item(list, item1);
            double_linked_list_unshift_item(list, item2);

            REQUIRE(list->head == item2);
            REQUIRE(list->tail == item1);
            REQUIRE(list->count == 2);
            REQUIRE(item2->prev == NULL);
            REQUIRE(item2->next == item1);
            REQUIRE(item1->prev == item2);
            REQUIRE(item1->next == NULL);

            double_linked_list_item_free(item1);
            double_linked_list_item_free(item2);
            double_linked_list_free(list);
        }
    }

    SECTION("double_linked_list_pop_item") {
        SECTION("pop one double_linked_list_item") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(5, {
                double_linked_list_item_t* item;

                item = double_linked_list_pop_item(list);

                REQUIRE(list->head == items[0]);
                REQUIRE(list->tail == items[3]);
                REQUIRE(list->count == 4);
                REQUIRE(item->prev == NULL);
                REQUIRE(item->next == NULL);
            });
        }

        SECTION("pop two items") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(5, {
                double_linked_list_item_t* item;

                item = double_linked_list_pop_item(list);
                item = double_linked_list_pop_item(list);

                REQUIRE(list->head == items[0]);
                REQUIRE(list->tail == items[2]);
                REQUIRE(list->count == 3);
                REQUIRE(item->prev == NULL);
                REQUIRE(item->next == NULL);
            });
        }

        SECTION("pop all items") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(5, {
                double_linked_list_item_t* item;
                for(int i = 0; i < 5; i++) {
                    item = double_linked_list_pop_item(list);
                }

                REQUIRE(list->head == NULL);
                REQUIRE(list->tail == NULL);
                REQUIRE(list->count == 0);
            });
        }
    }

    SECTION("double_linked_list_shift_item") {
        SECTION("pop one double_linked_list_item") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(5, {
                double_linked_list_item_t* item;

                item = double_linked_list_shift_item(list);

                REQUIRE(list->head == items[1]);
                REQUIRE(list->tail == items[4]);
                REQUIRE(list->count == 4);
                REQUIRE(item->prev == NULL);
                REQUIRE(item->next == NULL);
            });
        }

        SECTION("pop two items") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(5, {
                double_linked_list_item_t* item;

                item = double_linked_list_shift_item(list);
                item = double_linked_list_shift_item(list);

                REQUIRE(list->head == items[2]);
                REQUIRE(list->tail == items[4]);
                REQUIRE(list->count == 3);
                REQUIRE(item->prev == NULL);
                REQUIRE(item->next == NULL);
            });
        }

        SECTION("pop all items") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(5, {
                double_linked_list_item_t* item;
                for(int i = 0; i < 5; i++) {
                    item = double_linked_list_shift_item(list);
                }

                REQUIRE(list->head == NULL);
                REQUIRE(list->tail == NULL);
                REQUIRE(list->count == 0);
            });
        }
    }

    SECTION("double_linked_list_remove_item") {
        SECTION("remove from beginning") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(3, {
                double_linked_list_remove_item(list, items[0]);

                REQUIRE(list->head == items[1]);
                REQUIRE(list->tail == items[2]);
                REQUIRE(list->count == 2);
                REQUIRE(items[0]->prev == NULL);
                REQUIRE(items[0]->next == NULL);
                REQUIRE(items[1]->prev == NULL);
                REQUIRE(items[1]->next == items[2]);
                REQUIRE(items[2]->prev == items[1]);
                REQUIRE(items[2]->next == NULL);
            });
        }

        SECTION("remove from end") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(3, {
                double_linked_list_remove_item(list, items[2]);

                REQUIRE(list->head == items[0]);
                REQUIRE(list->tail == items[1]);
                REQUIRE(list->count == 2);
                REQUIRE(items[2]->prev == NULL);
                REQUIRE(items[2]->next == NULL);
                REQUIRE(items[0]->prev == NULL);
                REQUIRE(items[0]->next == items[1]);
                REQUIRE(items[1]->prev == items[0]);
                REQUIRE(items[1]->next == NULL);
            });
        }

        SECTION("remove from middle") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(3, {
                double_linked_list_remove_item(list, items[1]);

                REQUIRE(list->head == items[0]);
                REQUIRE(list->tail == items[2]);
                REQUIRE(list->count == 2);
                REQUIRE(items[1]->prev == NULL);
                REQUIRE(items[1]->next == NULL);
                REQUIRE(items[0]->prev == NULL);
                REQUIRE(items[0]->next == items[2]);
                REQUIRE(items[2]->prev == items[0]);
                REQUIRE(items[2]->next == NULL);
            });
        }
    }

    SECTION("double_linked_list_iter_next") {
        SECTION("iterate over full list") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(10, {
                int count = 0;
                double_linked_list_item_t* item = NULL;
                while((item = double_linked_list_iter_next(list, item)) != NULL) {
                    REQUIRE(item == items[count]);
                    count++;
                }

                REQUIRE(count == 10);
                REQUIRE(item == NULL);
            });
        }

        SECTION("iterate over empty list") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(0, {
                double_linked_list_item_t* item = NULL;
                REQUIRE(double_linked_list_iter_next(list, item) == NULL);
            });
        }
    }

    SECTION("double_linked_list_iter_prev") {
        SECTION("iterate over full list") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(10, {
                int count = 0;
                double_linked_list_item_t* item = NULL;
                while((item = double_linked_list_iter_prev(list, item)) != NULL) {
                    REQUIRE(item == items[items_length - count - 1]);
                    count++;
                }

                REQUIRE(count == 10);
                REQUIRE(item == NULL);
            });
        }

        SECTION("iterate over empty list") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(0, {
                double_linked_list_item_t* item = NULL;
                REQUIRE(double_linked_list_iter_prev(list, item) == NULL);
            });
        }
    }

    SECTION("double_linked_list_move_item_to_head") {
        SECTION("move tail to head") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(3, {
                double_linked_list_move_item_to_head(list, items[2]);

                REQUIRE(list->head == items[2]);
                REQUIRE(list->tail == items[1]);
                REQUIRE(list->count == 3);
                REQUIRE(items[2]->prev == NULL);
                REQUIRE(items[2]->next == items[0]);
                REQUIRE(items[0]->prev == items[2]);
                REQUIRE(items[0]->next == items[1]);
                REQUIRE(items[1]->prev == items[0]);
                REQUIRE(items[1]->next == NULL);
            });
        }

        SECTION("move head to head") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(3, {
                double_linked_list_move_item_to_head(list, items[0]);

                REQUIRE(list->head == items[0]);
                REQUIRE(list->tail == items[2]);
                REQUIRE(list->count == 3);
                REQUIRE(items[0]->prev == NULL);
                REQUIRE(items[0]->next == items[1]);
                REQUIRE(items[1]->prev == items[0]);
                REQUIRE(items[1]->next == items[2]);
                REQUIRE(items[2]->prev == items[1]);
                REQUIRE(items[2]->next == NULL);
            });
        }

        SECTION("move middle to head") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(3, {
                double_linked_list_move_item_to_head(list, items[1]);

                REQUIRE(list->head == items[1]);
                REQUIRE(list->tail == items[2]);
                REQUIRE(list->count == 3);
                REQUIRE(items[1]->prev == NULL);
                REQUIRE(items[1]->next == items[0]);
                REQUIRE(items[0]->prev == items[1]);
                REQUIRE(items[0]->next == items[2]);
                REQUIRE(items[2]->prev == items[0]);
                REQUIRE(items[2]->next == NULL);
            });
        }
    }

    SECTION("double_linked_list_move_item_to_tail") {
        SECTION("move tail to tail") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(3, {
                double_linked_list_move_item_to_tail(list, items[2]);

                REQUIRE(list->head == items[0]);
                REQUIRE(list->tail == items[2]);
                REQUIRE(list->count == 3);
                REQUIRE(items[0]->prev == NULL);
                REQUIRE(items[0]->next == items[1]);
                REQUIRE(items[1]->prev == items[0]);
                REQUIRE(items[1]->next == items[2]);
                REQUIRE(items[2]->prev == items[1]);
                REQUIRE(items[2]->next == NULL);
            });
        }

        SECTION("move head to tail") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(3, {
                double_linked_list_move_item_to_tail(list, items[0]);

                REQUIRE(list->head == items[1]);
                REQUIRE(list->tail == items[0]);
                REQUIRE(list->count == 3);
                REQUIRE(items[1]->prev == NULL);
                REQUIRE(items[1]->next == items[2]);
                REQUIRE(items[2]->prev == items[1]);
                REQUIRE(items[2]->next == items[0]);
                REQUIRE(items[0]->prev == items[2]);
                REQUIRE(items[0]->next == NULL);
            });
        }

        SECTION("move middle to tail") {
            TEST_DOUBLE_LINKED_LIST_FIXTURE_ALLOCATE(3, {
                double_linked_list_move_item_to_tail(list, items[1]);

                REQUIRE(list->head == items[0]);
                REQUIRE(list->tail == items[1]);
                REQUIRE(list->count == 3);
                REQUIRE(items[0]->prev == NULL);
                REQUIRE(items[0]->next == items[2]);
                REQUIRE(items[2]->prev == items[0]);
                REQUIRE(items[2]->next == items[1]);
                REQUIRE(items[1]->prev == items[2]);
                REQUIRE(items[1]->next == NULL);
            });
        }
    }
}
