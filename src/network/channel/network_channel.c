#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>

#include "exttypes.h"
#include "xalloc.h"

#include "network_channel_iouring.h"

#include "network_channel.h"

network_channel_config_t* network_channel_config_init() {
    return (network_channel_config_t*)xalloc_alloc(sizeof(network_channel_config_t));
}

network_channel_t* network_channel_init() {
    return (network_channel_t*)xalloc_alloc(sizeof(network_channel_t));
}
