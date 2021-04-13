#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <arpa/inet.h>

#include "exttypes.h"
#include "misc.h"
#include "xalloc.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "worker/worker.h"

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
