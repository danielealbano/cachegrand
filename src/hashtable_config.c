#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "xalloc.h"

#include "hashtable.h"
#include "hashtable_config.h"

hashtable_config_t* hashtable_config_init() {
    return (hashtable_config_t*)xalloc(sizeof(hashtable_config_t));
}

void hashtable_config_free(hashtable_config_t* hashtable_config) {
    xfree(hashtable_config);
}
