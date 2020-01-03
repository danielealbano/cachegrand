#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <t1ha.h>

#include "hashtable.h"
#include "hashtable_support.h"

bool hashtable_primenumbers_supported(uint64_t number) {
    return number <= HASHTABLE_PRIMENUMBERS_MAX ? true : false;
}

uint64_t hashtable_primenumbers_next(uint64_t number) {
    if (number < 53U) {
        return 53U;
    } else if (number < 101U) {
        return 101U;
    } else if (number < 307U) {
        return 307U;
    } else if (number < 677U) {
        return 677U;
    } else if (number < 1523U) {
        return 1523U;
    } else if (number < 3389U) {
        return 3389U;
    } else if (number < 7639U) {
        return 7639U;
    } else if (number < 17203U) {
        return 17203U;
    } else if (number < 26813U) {
        return 26813U;
    } else if (number < 40231U) {
        return 40231U;
    } else if (number < 60353U) {
        return 60353U;
    } else if (number < 90529U) {
        return 90529U;
    } else if (number < 135799U) {
        return 135799U;
    } else if (number < 203713U) {
        return 203713U;
    } else if (number < 305581U) {
        return 305581U;
    } else if (number < 458377U) {
        return 458377U;
    } else if (number < 687581U) {
        return 687581U;
    } else if (number < 1031399U) {
        return 1031399U;
    } else if (number < 1547101U) {
        return 1547101U;
    } else if (number < 2320651U) {
        return 2320651U;
    } else if (number < 5221501U) {
        return 5221501U;
    } else if (number < 7832261U) {
        return 7832261U;
    } else if (number < 11748391U) {
        return 11748391U;
    } else if (number < 17622589U) {
        return 17622589U;
    } else if (number < 26433887U) {
        return 26433887U;
    } else if (number < 39650833U) {
        return 39650833U;
    } else if (number < 59476253U) {
        return 59476253U;
    } else if (number < 89214403U) {
        return 89214403U;
    } else if (number < 133821673U) {
        return 133821673U;
    } else if (number < 200732527U) {
        return 200732527U;
    } else if (number < 301098823U) {
        return 301098823U;
    } else if (number < 451648247U) {
        return 451648247U;
    } else if (number < 677472371U) {
        return 677472371U;
    } else if (number < 1016208581U) {
        return 1016208581U;
    } else if (number < 1524312899U) {
        return 1524312899U;
    } else if (number < 2286469357U) {
        return 2286469357U;
    } else if (number < 3429704039U) {
        return 3429704039U;
    } else if (number < 5144556059U) {
        return 5144556059U;
    }

    return 0;
}

uint64_t hashtable_primenumbers_mod(uint64_t number, uint64_t prime) {
    if (prime == 53U) {
        return number % 53U;
    } else if (prime == 101U) {
        return number % 101U;
    } else if (prime == 307U) {
        return number % 307U;
    } else if (prime == 677U) {
        return number % 677U;
    } else if (prime == 1523U) {
        return number % 1523U;
    } else if (prime == 3389U) {
        return number % 3389U;
    } else if (prime == 7639U) {
        return number % 7639U;
    } else if (prime == 17203U) {
        return number % 17203U;
    } else if (prime == 26813U) {
        return number % 26813U;
    } else if (prime == 40231U) {
        return number % 40231U;
    } else if (prime == 60353U) {
        return number % 60353U;
    } else if (prime == 90529U) {
        return number % 90529U;
    } else if (prime == 135799U) {
        return number % 135799U;
    } else if (prime == 203713U) {
        return number % 203713U;
    } else if (prime == 305581U) {
        return number % 305581U;
    } else if (prime == 458377U) {
        return number % 458377U;
    } else if (prime == 687581U) {
        return number % 687581U;
    } else if (prime == 1031399U) {
        return number % 1031399U;
    } else if (prime == 1547101U) {
        return number % 1547101U;
    } else if (prime == 2320651U) {
        return number % 2320651U;
    } else if (prime == 5221501U) {
        return number % 5221501U;
    } else if (prime == 7832261U) {
        return number % 7832261U;
    } else if (prime == 11748391U) {
        return number % 11748391U;
    } else if (prime == 17622589U) {
        return number % 17622589U;
    } else if (prime == 26433887U) {
        return number % 26433887U;
    } else if (prime == 39650833U) {
        return number % 39650833U;
    } else if (prime == 59476253U) {
        return number % 59476253U;
    } else if (prime == 89214403U) {
        return number % 89214403U;
    } else if (prime == 133821673U) {
        return number % 133821673U;
    } else if (prime == 200732527U) {
        return number % 200732527U;
    } else if (prime == 301098823U) {
        return number % 301098823U;
    } else if (prime == 451648247U) {
        return number % 451648247U;
    } else if (prime == 677472371U) {
        return number % 677472371U;
    } else if (prime == 1016208581U) {
        return number % 1016208581U;
    } else if (prime == 1524312899U) {
        return number % 1524312899U;
    } else if (prime == 2286469357U) {
        return number % 2286469357U;
    } else if (prime == 3429704039U) {
        return number % 3429704039U;
    } else if (prime == 5144556059U) {
        return number % 5144556059U;
    }

    return 0;
}

uint64_t hashtable_rounddown_to_cacheline(uint64_t number) {
    return number -
           (number % HASHTABLE_HASHES_PER_CACHELINE);
}

uint64_t hashtable_roundup_to_cacheline_plus_one(uint64_t number) {
    return
            hashtable_rounddown_to_cacheline(number) +
            HASHTABLE_HASHES_PER_CACHELINE +
            HASHTABLE_HASHES_PER_CACHELINE;
}

hashtable_bucket_hash_t hashtable_calculate_hash(
        hashtable_key_data_t* key,
        hashtable_key_size_t key_size) {
    return t1ha2_atonce(key, key_size, HASHTABLE_T1HA2_SEED);
}

hashtable_bucket_index_t hashtable_bucket_index_from_hash(
        hashtable_bucket_count_t buckets_count,
        hashtable_bucket_hash_t hash) {
    return hashtable_primenumbers_mod(hash, buckets_count);
}

void hashtable_calculate_neighborhood(
        hashtable_bucket_count_t buckets_count,
        hashtable_bucket_hash_t hash,
        hashtable_bucket_index_t* index_neighborhood_begin,
        hashtable_bucket_index_t* index_neighborhood_end) {
    uint64_t index = hashtable_bucket_index_from_hash(buckets_count, hash);
    *index_neighborhood_begin = hashtable_rounddown_to_cacheline(index);
    *index_neighborhood_end = hashtable_roundup_to_cacheline_plus_one(index);
}
