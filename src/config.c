#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "xalloc.h"
#include "network/protocol/network_protocol.h"

#include "config.h"

config_t* config_init() {
    return (config_t*)xalloc_alloc_zero(sizeof(config_t));
}

void config_free(
        config_t* config) {

}

bool config_load(
        config_t* config,
        char* config_path,
        size_t config_path_len) {

    // TODO

    return false;
}
