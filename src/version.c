#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/utsname.h>

#include "misc.h"
#include "log.h"

#include "version.h"

LOG_PRODUCER_CREATE_LOCAL_DEFAULT("versiom", misc)

int misc_version_parse(
        char *release_string,
        long *version,
        size_t version_size) {
    char *token, *saveptr, *str;
    uint8_t version_part_index;
    uint32_t version_parts_count_max;

    version_parts_count_max = version_size / sizeof(long);
    if (version_parts_count_max > version_part_index) {
        version_parts_count_max = UINT8_MAX;
    }

    for(
            version_part_index = 0, str = release_string;
            version_part_index < version_parts_count_max;
            version_part_index++, str = NULL) {
        token = strtok_r(str, ".-", &saveptr);
        if (token == NULL)
            break;
        version[version_part_index] = strtol(token, NULL, 10);
    }

    return version_part_index;
}

int misc_version_compare(
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

bool misc_version_kernel_min(
        long min_kernel_version[4]) {
    long kernel_version[4];
    uint8_t versions_count;
    struct utsname utsname;
    uname(&utsname);

    if ((versions_count = misc_version_parse(
            utsname.release,
            kernel_version,
            sizeof(kernel_version))) < 4) {
        LOG_E(
                LOG_PRODUCER_DEFAULT,
                "Error parsing the version string <%s>, expected at least <4> parts in the version, found <%u>",
                utsname.release,
                versions_count);
        return false;
    }

    return misc_version_compare(
            min_kernel_version,
            kernel_version,
            4) >= 0;
}
