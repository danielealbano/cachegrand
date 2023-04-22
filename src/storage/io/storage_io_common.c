/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/uio.h>
#include <unistd.h>

#include "misc.h"
#include "log/log.h"

#include "storage_io_common.h"

#define TAG "storage_io_common"

bool storage_io_common_close(
        storage_io_common_fd_t fd) {
    if (close(fd)) {
        LOG_E(TAG, "Error closing the file descriptor with fd <%d>", fd);
        LOG_E_OS_ERROR(TAG);
        return false;
    }

    return true;
}
