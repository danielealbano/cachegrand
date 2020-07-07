#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/utsname.h>

#include "misc.h"
#include "log.h"
#include "xalloc.h"

#include "version.h"

LOG_PRODUCER_CREATE_LOCAL_DEFAULT("version", misc)

int version_parse(
        char release_string[],
        long *version,
        size_t version_size) {
    char *token, *saveptr = NULL;
    int version_parts_count_max, version_part_index = 0;

    version_parts_count_max = (int)(version_size / sizeof(long));
    if (version_parts_count_max > UINT8_MAX) {
        version_parts_count_max = UINT8_MAX;
    }

    char *str = xalloc_alloc(strlen(release_string) + 1);
    strcpy(str, release_string);

    for(token = strtok_r(str, ".-", &saveptr);
        (token != NULL) && (version_part_index < version_parts_count_max);
        token = strtok_r(NULL, ".-", &saveptr)) {
        char *number_out = NULL;
        long number = strtol(token, &number_out, 10);

        if (number_out == token) {
            break;
        }

        version[version_part_index] = number;
        version_part_index++;
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
        int parts_count) {
    uint8_t versions_count;
    struct utsname utsname;

    uname(&utsname);

    if ((versions_count = version_parse(
            utsname.release,
            kernel_version,
            sizeof(long) * parts_count)) < 3) {
        LOG_E(
                LOG_PRODUCER_DEFAULT,
                "Error parsing the version string <%s>, expected at least <3> parts in the version, found <%u>",
                utsname.release,
                versions_count);
        return false;
    }

    return true;
}

bool version_kernel_min(
        long *min_kernel_version,
        int parts_count) {
    long kernel_version[4];

    if (!version_kernel(
            (long*)&kernel_version,
            parts_count)) {
        return false;
    }

    return version_compare(
            min_kernel_version,
            kernel_version,
            parts_count) >= 0;
}
