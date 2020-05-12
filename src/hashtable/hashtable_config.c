#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "xalloc.h"

#include "hashtable.h"
#include "hashtable_config.h"
#include "hashtable_support_primenumbers.h"

// TODO: a BST can be implemented to fetch the cachelines_to_probe to apply looking for the hashtable_size
//       greater or equal to the primenumber
void hashtable_config_prefill_cachelines_to_probe_with_defaults(hashtable_config_t* hashtable_config) {
    memset((void*)hashtable_config->cachelines_to_probe, 0, sizeof(hashtable_config->cachelines_to_probe));

    HASHTABLE_PRIMENUMBERS_FOREACH(primenumbers, primenumbers_index, primenumber, {
        uint16_t cachelines_to_probe = 0;
        HASHTABLE_CONFIG_CACHELINES_PRIMENUMBERS_MAP_FOREACH(map, map_index, map_value, {
            cachelines_to_probe = map_value.cachelines_to_probe;
            if (map_value.hashtable_size >= primenumber) {
                break;
            }
        })

        hashtable_config->cachelines_to_probe[primenumbers_index].hashtable_size = primenumber;
        hashtable_config->cachelines_to_probe[primenumbers_index].cachelines_to_probe = cachelines_to_probe;
    })
}

hashtable_config_t* hashtable_config_init() {
    hashtable_config_t* hashtable_config = (hashtable_config_t*)xalloc_alloc(sizeof(hashtable_config_t));

    hashtable_config_prefill_cachelines_to_probe_with_defaults(hashtable_config);

    return hashtable_config;
}

void hashtable_config_free(hashtable_config_t* hashtable_config) {
    xalloc_free(hashtable_config);
}
