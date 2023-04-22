/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/utsname.h>

#include "misc.h"
#include "log/log.h"
#include "xalloc.h"

#include "version.h"

#define TAG "version"

int version_parse(
        char version_string[],
        long *version,
        uint8_t version_parts_count) {
    char *token, *saveptr = NULL;
    uint8_t version_part_index = 0;

    char *str = xalloc_alloc(strlen(version_string) + 1);
    strcpy(str, version_string);

    for(token = strtok_r(str, ".-", &saveptr);
        (token != NULL) && (version_part_index < version_parts_count);
        token = strtok_r(NULL, ".-", &saveptr)) {
        char *number_out = NULL;
        long number = strtol(token, &number_out, 10);

        if (number_out == token) {
            break;
        }

        version[version_part_index++] = number;
    }

    xalloc_free(str);

    return version_part_index;
}

int version_compare(
        const long version1[],
        const long version2[],
        uint8_t version_parts_count) {
    for(
            uint8_t version_part_index = 0;
            version_part_index < version_parts_count;
            version_part_index++) {
        if (version1[version_part_index] > version2[version_part_index]) {
            return 1;
        } else if (version1[version_part_index] < version2[version_part_index]) {
            return -1;
        }
    }

    return 0;
}

bool version_kernel(
        long *kernel_version,
        uint8_t version_parts_count) {
    uint8_t versions_count;
    struct utsname utsname;

    uname(&utsname);

    if ((versions_count = version_parse(
            utsname.release,
            kernel_version,
            version_parts_count)) != version_parts_count) {
        LOG_E(
                TAG,
                "Error parsing the version string <%s>, expected at <%d> parts in the version, found <%u>",
                utsname.release,
                version_parts_count,
                versions_count);
        return false;
    }

    return true;
}

bool version_kernel_min(
        long *min_kernel_version,
        uint8_t version_parts_count) {
    int res;
    long *kernel_version;

    kernel_version = xalloc_alloc_zero(sizeof(long) * version_parts_count);

    if (!version_kernel(
            kernel_version,
            version_parts_count)) {
        return false;
    }

    res = version_compare(
            kernel_version,
            min_kernel_version,
            version_parts_count) >= 0;

    xalloc_free(kernel_version);

    return res;
}
