#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "exttypes.h"
#include "spinlock.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_index.h"
#include "hashtable/hashtable_support_primenumbers.h"

hashtable_bucket_index_t hashtable_support_index_from_hash(
        hashtable_bucket_count_t buckets_count,
        hashtable_hash_t hash) {
    return hashtable_support_primenumbers_mod(hash, buckets_count);
}
