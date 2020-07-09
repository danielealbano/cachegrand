#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "misc.h"
#include "log.h"
#include "version.h"

#include "network_io_iouring_supported.h"

const char* kallsyms_path = "/proc/kallsyms";
const char* expected_symbol_name = "io_uring_create";

LOG_PRODUCER_CREATE_LOCAL_DEFAULT("network_io_iouring_supported", network_io_iouring_supported)

bool network_io_iouring_supported_fetch_kallsyms_symbol_name(
        FILE* fd,
        char* buffer,
        size_t buffer_size) {
    int res;
    unsigned long long address;
    char type;

    if (buffer_size < 500) {
        return false;
    }

    if ((res = fscanf(fd, "%llx %c %499s\n", &address, &type, buffer)) != 3) {
        if (res != EOF && fgets(buffer, 500, fd) == NULL) {
            LOG_E(LOG_PRODUCER_DEFAULT, "Unable to read data from <%s>", kallsyms_path);
            return false;
        }
    }

    return true;
}

bool network_io_iouring_supported_is_expected_symbol_name(
        char* buffer,
        size_t buffer_size) {
    return strncmp(buffer, expected_symbol_name, buffer_size) == 0;
}

FILE* network_io_iouring_supported_open_kallsyms() {
    FILE *fd = fopen(kallsyms_path, "r");

    if (fd == NULL) {
        LOG_E(LOG_PRODUCER_DEFAULT, "Unable to open <%s>", kallsyms_path);
        LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);
    }

    return fd;
}

bool network_io_iouring_supported() {
    bool ret = false;
    char name[500] = {0};
    long kernel_version[4] = {0};
    FILE *fd;

    // Check kernel minimum version, the io_uring probe op has been introduced in the kernel version 5.6
    version_parse("5.6.0", (long*)kernel_version, sizeof(kernel_version));
    version_kernel_min(kernel_version, 3);

    // Check that the io_uring symbols are exposes by the kernel
    if ((fd = network_io_iouring_supported_open_kallsyms()) == NULL) {
        return ret;
    }

    while(!feof(fd)) {
        if (network_io_iouring_supported_fetch_kallsyms_symbol_name(
                fd,
                name,
                sizeof(name)) == false) {
            break;
        }

        if (network_io_iouring_supported_is_expected_symbol_name(
                name,
                sizeof(name))) {
            ret = true;
            break;
        }
    }

    fclose(fd);

    return ret;
}
