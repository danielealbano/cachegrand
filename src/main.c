/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/resource.h>

#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "config.h"

#include "protocol/redis/protocol_redis_reader.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"
#include "signal_handler_thread.h"
#include "program.h"

int main(
        int argc,
        char** argv) {
    return program_main(argc, argv);
}
