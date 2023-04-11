/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
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

