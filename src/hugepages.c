/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <error.h>
#include <errno.h>

#include "misc.h"
#include "log/log.h"
#include "support/simple_file_io.h"

#include "hugepages.h"

#define TAG "hugepages"

bool hugepages_2mb_is_available(
        int min_available) {
    uint32_t nr_hugepages = simple_file_io_read_uint32_return(
            HUGEPAGES_SYSFS_2MB_PATH HUGEPAGES_SYSFS_FREE_HUGEPAGES_FILENAME);
    uint32_t resv_hugepages = simple_file_io_read_uint32_return(
            HUGEPAGES_SYSFS_2MB_PATH HUGEPAGES_SYSFS_RESV_HUGEPAGES_FILENAME);

    return nr_hugepages - resv_hugepages > min_available;
}

bool hugepages_1024mb_is_available(
        int min_available) {
    uint32_t nr_hugepages = simple_file_io_read_uint32_return(
            HUGEPAGES_SYSFS_1024MB_PATH HUGEPAGES_SYSFS_FREE_HUGEPAGES_FILENAME);
    uint32_t resv_hugepages = simple_file_io_read_uint32_return(
            HUGEPAGES_SYSFS_1024MB_PATH HUGEPAGES_SYSFS_RESV_HUGEPAGES_FILENAME);

    return nr_hugepages - resv_hugepages > min_available;
}
