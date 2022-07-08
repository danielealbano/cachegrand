/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#define _GNU_SOURCE

#include <stdbool.h>
#include <sys/resource.h>
#include <liburing.h>

#include "misc.h"
#include "log/log.h"

#include "program_ulimit.h"

#define TAG "program_ulimit"

bool program_ulimit_wrapper(
        __rlimit_resource_t resource,
        ulong value) {
    struct rlimit limit;

    limit.rlim_cur = value;
    limit.rlim_max = value;

    int result;
    result = setrlimit(resource, &limit);

    return result == 0;
}

bool program_ulimit_set_nofile(
        ulong value) {
    LOG_V(TAG, "> Setting max opened file ulimit to %lu", value);
    if (program_ulimit_wrapper(RLIMIT_NOFILE, value) == false) {
        LOG_W(TAG, "Unable to set max opened file ulimit");
        return false;
    }

    return true;
}

bool program_ulimit_set_memlock(
        ulong value) {
    LOG_V(TAG, "> Setting max lockable memory ulimit to %lu", value);
    if (program_ulimit_wrapper(RLIMIT_MEMLOCK, value) == false) {
        LOG_W(TAG, "Unable to set the lockable memory ulimit");
        return false;
    }

    return true;
}

void program_ulimit_setup() {
    LOG_V(TAG, "Configuring process ulimits");

    // TODO: this should come from the config but 0x80000 (524288) is a value extremely high that will cover for all
    //       the practical use cases, the current cachegrand storage architecture uses a small amount of file
    //       descriptors therefore the vast majority are for the network and with such a high number a system should
    //       be able to handle more than half a million of active connections (taking into account the linger time
    //       more than 15 IP addresses should be used before saturating the file descriptors).
    program_ulimit_set_nofile(PROGRAM_ULIMIT_NOFILE);
    program_ulimit_set_memlock(PROGRAM_ULIMIT_MEMLOCK);
}