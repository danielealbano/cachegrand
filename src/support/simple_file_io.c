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
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "misc.h"
#include "log/log.h"
#include "fatal.h"
#include "support/simple_file_io.h"

#define TAG "support/simple_file_io"

bool simple_file_io_exists(
        const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return false;
    }

    close(fd);
    return true;
}

bool simple_file_io_read(
        const char* path,
        char* out_data,
        size_t out_data_len) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        LOG_W(TAG, "Unable to open <%s>: %s", path, strerror(errno));
        return false;
    }

    // Subtract to ensure that there is enough space to add a string terminator
    ssize_t len = read(fd, out_data, out_data_len - 1);
    int read_errno = errno;
    close(fd);

    if (len < 0) {
        LOG_W(TAG, "Error while reading from <%s>: %s", path, strerror(read_errno));
        return false;
    }

    out_data[len] = '\0';

    return true;
}

bool simple_file_io_read_uint32(
        const char* path,
        uint32_t* out_data) {
    char* remaining_data;
    char data[256] = { 0 };

    if (!simple_file_io_read(path, data, sizeof(data))) {
        return false;
    }

    *out_data = strtol(data, &remaining_data, 0);

    // A new line is expected
    if (*remaining_data != 0x0a || data == remaining_data) {
        LOG_E(TAG, "Unable to read %s: invalid number", path);
        return false;
    }

    return true;
}

uint32_t simple_file_io_read_uint32_return(
        const char* path) {
    uint32_t value;
    if (!simple_file_io_read_uint32(path, &value)) {
        return 0;
    }

    return value;
}
