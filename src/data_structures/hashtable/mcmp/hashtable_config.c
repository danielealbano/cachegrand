/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <numa.h>

#include "exttypes.h"
#include "spinlock.h"
#include "transaction.h"
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
