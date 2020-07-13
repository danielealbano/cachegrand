#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <liburing.h>

#include "misc.h"
#include "log.h"
#include "version.h"

#include "io_uring_support.h"
#include "io_uring_supported.h"

const char* kallsyms_path = "/proc/kallsyms";
const char* expected_symbol_name = "io_uring_setup";

LOG_PRODUCER_CREATE_DEFAULT("io_uring_supported", io_uring_supported)

bool io_uring_supported_fetch_kallsyms_symbol_name(
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

bool io_uring_supported_is_expected_symbol_name(
        char* buffer,
        size_t buffer_size) {
    return strstr(buffer, expected_symbol_name) != NULL;
}

FILE* io_uring_supported_open_kallsyms() {
    FILE *fd = fopen(kallsyms_path, "r");

    if (fd == NULL) {
        LOG_E(LOG_PRODUCER_DEFAULT, "Unable to open <%s>", kallsyms_path);
        LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);
    }

    return fd;
}

bool io_uring_supported() {
    bool ret = false;
    char name[500] = {0};
    long kernel_version[4] = {0};
    FILE *fd;

    // Check kernel minimum version, the io_uring probe op has been introduced in the kernel version 5.7
    version_parse("5.7.0", (long*)kernel_version, sizeof(kernel_version));
    if (!version_kernel_min(kernel_version, 3)) {
        return false;
    }

    // Check that the io_uring symbols are exposes by the kernel
    if ((fd = io_uring_supported_open_kallsyms()) == NULL) {
        return ret;
    }

    while(!feof(fd)) {
        if (io_uring_supported_fetch_kallsyms_symbol_name(
                fd,
                name,
                sizeof(name)) == false) {
            break;
        }

        if (io_uring_supported_is_expected_symbol_name(
                name,
                sizeof(name))) {
            ret = true;
            break;
        }
    }

    fclose(fd);

    if (ret) {
        ret = io_uring_support_probe_opcode(IORING_FEAT_FAST_POLL);
    }

    return ret;
}
