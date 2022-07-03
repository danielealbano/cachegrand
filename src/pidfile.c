/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
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
#include <time.h>
#include <sys/file.h>
#include <linux/limits.h>

#include "misc.h"
#include "log/log.h"
#include "fatal.h"
#include "support/simple_file_io.h"
#include "pidfile.h"

#define TAG "pidfile"

bool pidfile_owned = false;
int pidfile_fd = -1;

bool pidfile_request_close_on_exec(
        int fd) {
    int flags;

    if ((flags = fcntl(fd, F_GETFD)) == -1) {
        return false;
    }

    flags |= FD_CLOEXEC;
    if (fcntl(fd, F_SETFD, flags) == -1) {
        return false;
    }

    return true;
}

bool pidfile_request_lock(
        int fd) {
    struct flock lock = { 0 };

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    if (fcntl(fd, F_SETLK, &lock) == -1) {
        return false;
    }

    return true;
}

int pidfile_open(
        const char* pidfile_path) {
    return open(pidfile_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
}

bool pidfile_write_pid(
        int fd,
        long pid) {
    char buf[64];

    if (ftruncate(fd, 0) == -1) {
        return false;
    }

    snprintf(buf, sizeof(buf) - 1, "%ld\n", pid);
    if (write(fd, buf, strlen(buf)) != strlen(buf)) {
        return false;
    }

    return true;
}

bool pidfile_create(
        const char* pidfile_path) {
    int fd;

    if ((fd = pidfile_open(pidfile_path)) < 0) {
        LOG_E(TAG, "Failed to open the pid file <%s>", pidfile_path);
        LOG_E_OS_ERROR(TAG);
        return false;
    }

    if (!pidfile_request_close_on_exec(fd)) {
        LOG_E(TAG, "Failed to set the close-on-exec flag for the pid file <%s>", pidfile_path);
        LOG_E_OS_ERROR(TAG);
        pidfile_close(fd);
        return false;
    }

    if (!pidfile_request_lock(fd)) {
        LOG_E(
                TAG,
                "The pid file <%s> is owned by process id <%u>",
                pidfile_path,
                simple_file_io_read_uint32_return(pidfile_path));
        pidfile_close(fd);
        return false;
    }

    if (!pidfile_write_pid(fd, (long)getpid())) {
        LOG_E(TAG, "Failed write the pid to the pid file <%s>", pidfile_path);
        LOG_E_OS_ERROR(TAG);
        pidfile_close(fd);
        return false;
    }

    pidfile_owned = true;
    pidfile_fd = fd;

    return true;
}

bool pidfile_close(
        int fd) {
    // Reads the path to the fd at runtime to ensure that the correct one will be deleted if it gets moved around
    char fd_path[PATH_MAX] = { 0 };
    char pidfile_path[PATH_MAX] = { 0 };

    snprintf(fd_path, PATH_MAX - 1, "/proc/self/fd/%d", fd);

    size_t res = readlink(fd_path, pidfile_path, PATH_MAX - 1);

    close(fd);

    if (res != -1) {
        unlink(pidfile_path);
    }

    pidfile_fd = -1;
    pidfile_owned = false;

    return res != -1;
}

bool pidfile_is_owned() {
    return pidfile_owned;
}

int pidfile_get_fd() {
    return pidfile_fd;
}
