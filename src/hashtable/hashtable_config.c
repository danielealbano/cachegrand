#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "xalloc.h"

#include "hashtable.h"
#include "hashtable_config.h"
#include "hashtable_support_primenumbers.h"

hashtable_config_t* hashtable_config_init() {
    hashtable_config_t* hashtable_config = (hashtable_config_t*)xalloc_alloc(sizeof(hashtable_config_t));

    hashtable_config_prefill_cachelines_to_probe_with_defaults(hashtable_config);

    return hashtable_config;
}

void hashtable_config_free(hashtable_config_t* hashtable_config) {
    xalloc_free(hashtable_config);
}
