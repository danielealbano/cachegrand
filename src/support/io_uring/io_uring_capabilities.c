#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <liburing.h>

#include "misc.h"
#include "log.h"
#include "version.h"
#include "xalloc.h"

#include "io_uring_support.h"
#include "io_uring_capabilities.h"

const char* kallsyms_path = "/proc/kallsyms";
const char* expected_symbol_name = "io_uring_setup";
const char* minimum_kernel_version = "5.7.0";

#define TAG "io_uring_capabilities_is_supported"

bool io_uring_capabilities_kallsyms_fetch_symbol_name(
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
            LOG_E(TAG, "Unable to read data from <%s>", kallsyms_path);
            return false;
        }
    }

    return true;
}

bool io_uring_capabilities_kallsyms_is_expected_symbol_name(
        char* buffer,
        size_t buffer_size) {
    return strstr(buffer, expected_symbol_name) != NULL;
}

FILE* io_uring_capabilities_kallsyms_open() {
    FILE *fd = fopen(kallsyms_path, "r");

    if (fd == NULL) {
        LOG_E(TAG, "Unable to open <%s>", kallsyms_path);
        LOG_E_OS_ERROR(TAG);
    }

    return fd;
}

bool io_uring_capabilities_kallsyms_ensure_iouring_available() {
    FILE *fd;
    char name[500] = {0};
    bool ret = false;

    // Check that the io_uring symbols are exposes by the kernel
    if ((fd = io_uring_capabilities_kallsyms_open()) == NULL) {
        return ret;
    }

    while(!feof(fd)) {
        if (io_uring_capabilities_kallsyms_fetch_symbol_name(
                fd,
                name,
                sizeof(name)) == false) {
            break;
        }

        if (io_uring_capabilities_kallsyms_is_expected_symbol_name(
                name,
                sizeof(name))) {
            ret = true;
            break;
        }
    }

    fclose(fd);

    return ret;
}

bool io_uring_capabilities_is_linked_op_files_update_supported() {
    io_uring_t *ring;
    io_uring_sqe_t *sqe = NULL;
    io_uring_cqe_t *cqe1 = NULL, *cqe2 = NULL;
    int fd = 1;
    bool ret = false;
    uint32_t files_map_count = 16;
    const int *files_map_registered = xalloc_alloc_zero(sizeof(int) * files_map_count);

    if ((ring = io_uring_support_init(16, NULL, NULL)) == NULL) {
        return false;
    }

    if (io_uring_register_files(ring, files_map_registered, files_map_count) < 0) {
        io_uring_support_free(ring);
        return false;
    }

    sqe = io_uring_support_get_sqe(ring);
    io_uring_prep_files_update(sqe, &fd, 1, 10);
    io_uring_sqe_set_flags(sqe, IOSQE_IO_LINK);

    io_uring_support_sqe_enqueue_nop(ring, 0, 0);
    io_uring_support_sqe_submit(ring);

    io_uring_wait_cqe(ring, &cqe1);
    io_uring_cqe_seen(ring, cqe1);
    io_uring_wait_cqe(ring, &cqe2);
    io_uring_cqe_seen(ring, cqe2);

    return cqe1->res == 1 && cqe2->res == 0;
}

bool io_uring_capabilities_is_supported() {
    long kernel_version[3] = {0};

    // Check kernel minimum version, the io_uring probe op has been introduced in recent kernel versions and we also
    // need IORING_FEAT_FAST_POLL that is available only from the kernel version 5.7.0 onwards
    version_parse((char*)minimum_kernel_version, (long*)kernel_version, sizeof(kernel_version));
    if (!version_kernel_min(kernel_version, 3)) {
        return false;
    }

    // Check if the kernel has been compiled with io_uring support
    if (!io_uring_capabilities_kallsyms_ensure_iouring_available()) {
        return false;
    }

    // Check if fast poll is supported
    if (!io_uring_support_probe_opcode(IORING_FEAT_FAST_POLL)) {
        return false;
    }

    return true;
}
