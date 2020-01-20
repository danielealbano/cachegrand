#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "xalloc.h"
#include "hashtable.h"
#include "hashtable_config.h"
#include "hashtable_data.h"

hashtable_t* hashtable_init(hashtable_config_t* hashtable_config) {
    hashtable_t* hashtable = (hashtable_t*)xalloc(sizeof(hashtable_t));
    hashtable_data_t* hashtable_data = hashtable_data_init(hashtable_config->initial_size);

    hashtable->is_resizing = false;
    hashtable->is_shutdowning = false;
    hashtable->ht_current = hashtable_data;
    hashtable->ht_old = NULL;

    hashtable->config = hashtable_config;

    return hashtable;
}

void hashtable_free(hashtable_t* hashtable) {
    if (hashtable->ht_current) {
        hashtable_data_free(hashtable->ht_current);
        hashtable->ht_current = NULL;
    }

    if (hashtable->ht_old) {
        hashtable_data_free(hashtable->ht_old);
        hashtable->ht_old = NULL;
    }

    hashtable_config_free(hashtable->config);

    xfree(hashtable);
}
