#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <error.h>
#include <errno.h>

#include "log.h"

#include "hugepages.h"

#define TAG "hugepages"

bool hugepages_file_read(
        const char* path,
        char* out_data,
        size_t out_data_len) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        LOG_W(TAG, "Unable to open %s: %s", path, strerror(errno));
        return false;
    }

    size_t len = read(fd, out_data, out_data_len);
    int read_errno = errno;
    close(fd);

    if (len < 0) {
        LOG_W(TAG, "Error while reading from %s: %s", path, strerror(read_errno));
        return false;
    }

    out_data[len] = '\0';

    return true;
}

bool hugepages_file_read_uint32(
        const char* path,
        uint32_t* out_data) {
    char* remaining_data;
    char data[256] = { 0 };

    if (!hugepages_file_read(path, data, sizeof(data))) {
        return false;
    }

    *out_data = strtol(data, &remaining_data, 0);

    // A new line is expected
    if (*remaining_data != 0x0a) {
        LOG_E(TAG, "Unable to read %s: invalid number", path);
        return -1;
    }

    return true;
}

uint32_t hugepages_file_path_uint32_return(
        char* path) {
    uint32_t value;
    if (!hugepages_file_read_uint32(path, &value)) {
        return 0;
    }

    return value;
}

bool hugepages_2mb_is_available(
        int min_available) {
    uint32_t nr_hugepages = hugepages_file_path_uint32_return(
            HUGEPAGES_SYSFS_2MB_PATH HUGEPAGES_SYSFS_FREE_HUGEPAGES_FILENAME);
    uint32_t resv_hugepages = hugepages_file_path_uint32_return(
            HUGEPAGES_SYSFS_2MB_PATH HUGEPAGES_SYSFS_RESV_HUGEPAGES_FILENAME);

    return nr_hugepages - resv_hugepages > min_available;
}

bool hugepages_1024mb_is_available(
        int min_available) {
    uint32_t nr_hugepages = hugepages_file_path_uint32_return(
            HUGEPAGES_SYSFS_1024MB_PATH HUGEPAGES_SYSFS_FREE_HUGEPAGES_FILENAME);
    uint32_t resv_hugepages = hugepages_file_path_uint32_return(
        HUGEPAGES_SYSFS_1024MB_PATH HUGEPAGES_SYSFS_RESV_HUGEPAGES_FILENAME);

    return nr_hugepages - resv_hugepages > min_available;
}
