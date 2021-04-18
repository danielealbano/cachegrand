#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "exttypes.h"

#include "protocol/redis/protocol_redis_reader.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "worker/worker.h"
#include "config.h"

#include "program.h"

int main() {
    return program_main();
}
