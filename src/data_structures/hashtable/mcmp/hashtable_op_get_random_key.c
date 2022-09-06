/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <assert.h>
#include <string.h>

#include "random.h"
#include "misc.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/fast_fixed_memory_allocator.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_data.h"
#include "data_structures/hashtable/mcmp/hashtable_op_get_key.h"

#include "hashtable_op_get_random_key.h"

bool hashtable_mcmp_op_get_random_key_try(
        hashtable_t *hashtable,
        char **key,
        hashtable_key_size_t *key_size) {
    uint64_t random_value = random_generate();

    return hashtable_mcmp_op_get_key(
            hashtable,
            random_value % hashtable->ht_current->buckets_count_real,
            key,
            key_size);
}
