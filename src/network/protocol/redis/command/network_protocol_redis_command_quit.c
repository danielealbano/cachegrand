#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <liburing.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "support/io_uring/io_uring_support.h"
#include "network/protocol/redis/network_protocol_redis.h"

#define TAG "network_protocol_redis_command_quit"

NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_BEGIN(quit, {})
NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_ARGUMENT_PROCESSED(quit, {})
NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_END(quit, {
    NETWORK_PROTOCOL_REDIS_WRITE_ENSURE_NO_ERROR({
        send_buffer_start = protocol_redis_writer_write_blob_string(
                send_buffer_start,
                send_buffer_end - send_buffer_start,
                "OK",
                4);
        network_channel_user_data->close_connection_on_send = true;
    })
})
