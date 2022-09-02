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
#include <numa.h>

#include "exttypes.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"

#include "hashtable.h"
#include "hashtable_support_index.h"

hashtable_bucket_index_t hashtable_mcmp_support_index_from_hash(
        hashtable_bucket_count_t buckets_count,
        hashtable_hash_t hash) {
    return hash & (buckets_count - 1);
}
