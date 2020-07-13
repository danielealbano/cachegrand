#include "../catch.hpp"

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <liburing.h>

#include "exttypes.h"
#include "xalloc.h"
#include "log.h"

#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "worker/worker.h"
#include "io_uring_support.h"

#include "worker/worker_iouring.h"

TEST_CASE("worker/worker_iouring.c", "[worker][worker_iouring]") {
    // TODO: write tests when the worker interface will stabile, do not test anything right now, the worker needs to be
    //       heavily refactored and most of the code will be split, shuffled, rewritten and re-organized.
    //       The target would be to have these tests by time we will implement a second kind of worker (ie. for epoll).
}
