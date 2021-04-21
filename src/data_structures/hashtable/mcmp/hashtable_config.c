#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <numa.h>

#include "exttypes.h"
#include "spinlock.h"
#include "xalloc.h"

#include "hashtable.h"
#include "hashtable_config.h"

hashtable_config_t* hashtable_mcmp_config_init() {
    hashtable_config_t* hashtable_config = (hashtable_config_t*)xalloc_alloc_zero(sizeof(hashtable_config_t));

    return hashtable_config;
}

void hashtable_mcmp_config_free(hashtable_config_t* hashtable_config) {
    xalloc_free(hashtable_config);
}
