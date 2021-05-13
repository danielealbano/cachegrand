/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "xalloc.h"

#include "double_linked_list.h"

double_linked_list_item_t* double_linked_list_item_init() {
    return (double_linked_list_item_t*)xalloc_alloc_zero(sizeof(double_linked_list_item_t));
}

void double_linked_list_item_free(
        double_linked_list_item_t *item) {
    xalloc_free(item);
}

double_linked_list_t* double_linked_list_init() {
    return (double_linked_list_t*)xalloc_alloc_zero(sizeof(double_linked_list_t));
}

void double_linked_list_free(
        double_linked_list_t *list) {
    xalloc_free(list);
}

double_linked_list_item_t *double_linked_list_iter_next(
        double_linked_list_t *list,
        double_linked_list_item_t *current_item) {
    assert(list != NULL);

    if (current_item == NULL) {
        current_item = list->head;
    } else {
        current_item = current_item->next;
    }

    return current_item;
}

double_linked_list_item_t *double_linked_list_iter_prev(
        double_linked_list_t *list,
        double_linked_list_item_t *current_item) {
    assert(list != NULL);

    if (current_item == NULL) {
        current_item = list->tail;
    } else {
        current_item = current_item->prev;
    }

    return current_item;
}

void double_linked_list_insert_item_before(
        double_linked_list_t *list,
        double_linked_list_item_t *item,
        double_linked_list_item_t *before_item) {
    assert(item != NULL);
    assert(list != NULL);
    assert(before_item != NULL || (before_item == NULL && list->head == NULL));

    item->prev = NULL;
    item->next = NULL;

    if (before_item != NULL) {
        item->prev = before_item->prev;
        item->next = before_item;
        before_item->prev = item;

        if (item->prev && item->prev->next) {
            item->prev->next = item;
        }
    }

    if (item->prev == NULL) {
        list->head = item;
    }

    if (item->next == NULL) {
        list->tail = item;
    }

    list->count++;

}

void double_linked_list_insert_item_after(
        double_linked_list_t *list,
        double_linked_list_item_t *item,
        double_linked_list_item_t *after_item) {
    assert(item != NULL);
    assert(list != NULL);
    assert(after_item != NULL || (after_item == NULL && list->head == NULL));

    item->prev = NULL;
    item->next = NULL;

    if (after_item != NULL) {
        item->prev = after_item;
        item->next = after_item->next;
        after_item->next = item;

        if (item->next && item->next->prev) {
            item->next->prev = item;
        }
    }

    if (item->prev == NULL) {
        list->head = item;
    }

    if (item->next == NULL) {
        list->tail = item;
    }

    list->count++;
}

void double_linked_list_push_item(
        double_linked_list_t *list,
        double_linked_list_item_t *item) {
    assert(list != NULL);
    return double_linked_list_insert_item_after(list, item, list->tail);
}

double_linked_list_item_t *double_linked_list_pop_item(
        double_linked_list_t *list) {
    assert(list != NULL);

    double_linked_list_item_t* item = list->tail;

    if (item) {
        double_linked_list_remove_item(list, item);
    }

    return item;
}

void double_linked_list_unshift_item(
        double_linked_list_t *list,
        double_linked_list_item_t *item) {
    assert(list != NULL);
    return double_linked_list_insert_item_before(list, item, list->head);
}

double_linked_list_item_t *double_linked_list_shift_item(
        double_linked_list_t *list) {
    assert(list != NULL);

    double_linked_list_item_t* item = list->head;

    if (item) {
        double_linked_list_remove_item(list, item);
    }

    return item;
}

void double_linked_list_remove_item(
        double_linked_list_t *list,
        double_linked_list_item_t *item) {
    assert(item != NULL);
    assert(list != NULL);

    if (item->next) {
        item->next->prev = item->prev;
    }

    if (item->prev) {
        item->prev->next = item->next;
    }

    if (list->head == item) {
        list->head = item->next;
    }

    if (list->tail == item) {
        list->tail = item->prev;
    }

    item->next = NULL;
    item->prev = NULL;
    list->count--;
}

void double_linked_list_move_item_to_head(
        double_linked_list_t *list,
        double_linked_list_item_t *item) {
    assert(item != NULL);
    assert(list != NULL);
    assert(list->head != NULL && list->tail != NULL);

    double_linked_list_item_t* head;
    double_linked_list_item_t* item_prev;
    double_linked_list_item_t* item_next;

    if (list->head == item) {
        return;
    }

    head = list->head;
    item_prev = item->prev;
    item_next = item->next;

    // Update the head and, if the item is the tail, update the tail
    list->head = item;
    if (list->tail == item) {
        list->tail = item_prev;
    }

    // Set the next of the item and update the old head prev
    item->next = head;
    item->prev = NULL;
    if (head) {
        head->prev = item;
    }

    // Update the previous and/or the next item if they exist
    if (item_prev != NULL) {
        item_prev->next = item_next;
    }
    if (item_next != NULL) {
        item_next->prev = item_prev;
    }
}

void double_linked_list_move_item_to_tail(
        double_linked_list_t *list,
        double_linked_list_item_t *item) {
    assert(item != NULL);
    assert(list != NULL);
    assert(list->head != NULL && list->tail != NULL);

    double_linked_list_item_t* tail;
    double_linked_list_item_t* item_prev;
    double_linked_list_item_t* item_next;

    if (list->tail == item) {
        return;
    }

    tail = list->tail;
    item_prev = item->prev;
    item_next = item->next;

    // Update the tail and, if the item is the head, update the head to point to item_next
    list->tail = item;
    if (list->head == item) {
        list->head = item_next;
    }

    // Set the prev of the item and update the old tail next
    item->next = NULL;
    item->prev = tail;
    if (tail) {
        tail->next = item;
    }

    // Update the previous and/or the next item if they exist
    if (item_prev != NULL) {
        item_prev->next = item_next;
    }
    if (item_next) {
        item_next->prev = item_prev;
    }
}
