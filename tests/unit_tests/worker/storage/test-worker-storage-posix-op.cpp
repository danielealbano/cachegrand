/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "fiber/fiber.h"
#include "fiber/fiber_scheduler.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "config.h"
#include "clock.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "worker/storage/worker_storage_op.h"
#include "worker/storage/worker_storage_posix_op.h"

// Fiber and related user data struct to test the read operation
typedef struct test_worker_storage_posix_op_fiber_userdata test_worker_storage_posix_op_fiber_userdata_t;
struct test_worker_storage_posix_op_fiber_userdata {
    bool open;
    bool write;
    bool read;
    bool flush;
    bool fallocate;
    char *path;
    storage_io_common_open_flags_t open_flags;
    storage_io_common_open_mode_t open_mode;
    storage_io_common_iovec_t *iovec;
    size_t iovec_nr;
    off_t offset;
    off_t len;
    int fallocate_mode;
    off_t fallocate_offset;
    off_t fallocate_len;
    storage_channel_t **open_result;
    size_t *read_write_result;
    bool *flush_result;
    bool *fallocate_result;
};
void test_worker_storage_posix_op_fiber_entrypoint(void *user_data) {
    storage_channel_t *open_result;
    size_t read_write_result;
    bool flush_result;
    bool fallocate_result;

    test_worker_storage_posix_op_fiber_userdata_t *user_data_fiber =
            (test_worker_storage_posix_op_fiber_userdata_t*)user_data;

    if (user_data_fiber->open) {
        open_result = (storage_channel_t*)worker_storage_posix_op_storage_open(
                user_data_fiber->path,
                user_data_fiber->open_flags,
                user_data_fiber->open_mode);

        if (user_data_fiber->open_result) {
            *user_data_fiber->open_result = open_result;
        }
    }

    if (user_data_fiber->read) {
        read_write_result = worker_storage_posix_op_storage_read(
                (storage_channel_t *) *user_data_fiber->open_result,
                user_data_fiber->iovec,
                user_data_fiber->iovec_nr,
                user_data_fiber->offset);

        if (user_data_fiber->read_write_result) {
            *user_data_fiber->read_write_result = read_write_result;
        }
    }

    if (user_data_fiber->write) {
        read_write_result = worker_storage_posix_op_storage_write(
                (storage_channel_t *) *user_data_fiber->open_result,
                user_data_fiber->iovec,
                user_data_fiber->iovec_nr,
                user_data_fiber->offset);

        if (user_data_fiber->read_write_result) {
            *user_data_fiber->read_write_result = read_write_result;
        }
    }

    if (user_data_fiber->flush) {
        flush_result = worker_storage_posix_op_storage_flush(
                (storage_channel_t *) *user_data_fiber->open_result);

        if (user_data_fiber->flush_result) {
            *user_data_fiber->flush_result = flush_result;
        }
    }

    if (user_data_fiber->fallocate) {
        fallocate_result = worker_storage_posix_op_storage_fallocate(
                (storage_channel_t *) *user_data_fiber->open_result,
                user_data_fiber->fallocate_mode,
                user_data_fiber->fallocate_offset,
                user_data_fiber->fallocate_len);

        if (user_data_fiber->fallocate_result) {
            *user_data_fiber->fallocate_result = fallocate_result;
        }
    }

    fiber_scheduler_switch_back();
}

TEST_CASE("worker/storage/worker_storage_posix_op.c", "[worker][worker_storage][worker_storage_posix_op]") {
    storage_channel_t *storage_channel = NULL;

    char fixture_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
    int fixture_temp_path_suffix_len = 4;
    close(mkstemps(fixture_temp_path, fixture_temp_path_suffix_len));

    char buffer_write[] = "cachegrand test - read / write tests";
    char buffer_read1[128] = { 0 }, buffer_read2[128] = { 0 };
    struct iovec iovec[2] = { 0 };

    fiber_t *fiber = NULL;
    char fiber_name[] = "test-fiber";
    size_t fiber_name_len = strlen(fiber_name);

    SECTION("worker_storage_posix_op_storage_open") {
        SECTION("open an existing file") {
            test_worker_storage_posix_op_fiber_userdata_t user_data = {
                    .open = true,
                    .path = fixture_temp_path,
                    .open_flags = O_RDONLY,
                    .open_mode = 0,
                    .open_result = &storage_channel
            };
            fiber = fiber_scheduler_new_fiber(
                    fiber_name,
                    fiber_name_len,
                    test_worker_storage_posix_op_fiber_entrypoint,
                    &user_data);

            REQUIRE(user_data.open_result != NULL);
            REQUIRE(fiber->error_number == 0);
            REQUIRE(storage_channel->fd > -1);
            REQUIRE(strncmp(storage_channel->path, fixture_temp_path, strlen(fixture_temp_path)) == 0);
            REQUIRE(strlen(fixture_temp_path) == storage_channel->path_len);
        }

        SECTION("open a non-existing file creating it") {
            // The file gets pre-created for convenience during the test setup, need to be unlinked for the test to
            // be able to reuse the unique file name
            unlink(fixture_temp_path);
            test_worker_storage_posix_op_fiber_userdata_t user_data = {
                    .open = true,
                    .path = fixture_temp_path,
                    .open_flags = O_CREAT | O_RDWR | O_EXCL,
                    .open_mode = S_IRUSR | S_IWUSR,
                    .open_result = &storage_channel
            };
            fiber = fiber_scheduler_new_fiber(
                    fiber_name,
                    fiber_name_len,
                    test_worker_storage_posix_op_fiber_entrypoint,
                    &user_data);

            REQUIRE(user_data.open_result != NULL);
            REQUIRE(fiber->error_number == 0);
            REQUIRE(storage_channel->fd > -1);
            REQUIRE(strncmp(storage_channel->path, fixture_temp_path, strlen(fixture_temp_path)) == 0);
            REQUIRE(strlen(fixture_temp_path) == storage_channel->path_len);
        }

        SECTION("fail to open an non-existing file without create option") {
            // The file gets pre-created for convenience during the test setup, need to be unlinked for the test to
            // be able to reuse the unique file name
            unlink(fixture_temp_path);
            test_worker_storage_posix_op_fiber_userdata_t user_data = {
                    .open = true,
                    .path = fixture_temp_path,
                    .open_flags = O_RDONLY,
                    .open_mode = 0,
                    .open_result = &storage_channel
            };
            fiber = fiber_scheduler_new_fiber(
                    fiber_name,
                    fiber_name_len,
                    test_worker_storage_posix_op_fiber_entrypoint,
                    &user_data);

            REQUIRE(user_data.open_result != NULL);
            REQUIRE(fiber->error_number == ENOENT);
            REQUIRE(storage_channel == NULL);
        }
    }

    SECTION("worker_storage_posix_op_storage_read") {
        SECTION("read n. 1 iovec") {
            size_t len = 0;
            iovec[0].iov_base = buffer_read1;
            iovec[0].iov_len = strlen(buffer_write);

            int fd = openat(0, fixture_temp_path, O_WRONLY, 0);
            REQUIRE(fd > -1);
            REQUIRE(write(fd, buffer_write, strlen(buffer_write)) == strlen(buffer_write));
            REQUIRE(close(fd) == 0);

            test_worker_storage_posix_op_fiber_userdata_t user_data = {
                    .open = true,
                    .read = true,
                    .path = fixture_temp_path,
                    .open_flags = O_RDONLY,
                    .open_mode = 0,
                    .iovec = iovec,
                    .iovec_nr = 1,
                    .offset = 0,
                    .open_result = &storage_channel,
                    .read_write_result = &len,
            };
            fiber = fiber_scheduler_new_fiber(
                    fiber_name,
                    fiber_name_len,
                    test_worker_storage_posix_op_fiber_entrypoint,
                    &user_data);

            REQUIRE(fiber->error_number == 0);
            REQUIRE(len == strlen(buffer_write));
            REQUIRE(strncmp(buffer_write, buffer_read1, strlen(buffer_write)) == 0);
        }

        SECTION("read n. 2 iovec") {
            size_t len = 0;
            iovec[0].iov_base = buffer_read1;
            iovec[0].iov_len = strlen(buffer_write);
            iovec[1].iov_base = buffer_read2;
            iovec[1].iov_len = strlen(buffer_write);

            int fd = openat(0, fixture_temp_path, O_WRONLY, 0);
            REQUIRE(fd > -1);
            REQUIRE(write(fd, buffer_write, strlen(buffer_write)) == strlen(buffer_write));
            REQUIRE(write(fd, buffer_write, strlen(buffer_write)) == strlen(buffer_write));
            REQUIRE(close(fd) == 0);

            test_worker_storage_posix_op_fiber_userdata_t user_data = {
                    .open = true,
                    .read = true,
                    .path = fixture_temp_path,
                    .open_flags = O_RDONLY,
                    .open_mode = 0,
                    .iovec = iovec,
                    .iovec_nr = 2,
                    .offset = 0,
                    .open_result = &storage_channel,
                    .read_write_result = &len,
            };
            fiber = fiber_scheduler_new_fiber(
                    fiber_name,
                    fiber_name_len,
                    test_worker_storage_posix_op_fiber_entrypoint,
                    &user_data);

            REQUIRE(fiber->error_number == 0);
            REQUIRE(len == strlen(buffer_write) * 2);
            REQUIRE(strncmp(buffer_write, buffer_read1, strlen(buffer_write)) == 0);
            REQUIRE(strncmp(buffer_write, buffer_read2, strlen(buffer_write)) == 0);
        }

        SECTION("invalid fd") {
            size_t len = 0;
            iovec[0].iov_base = buffer_read1;
            iovec[0].iov_len = strlen(buffer_write);

            storage_channel_t storage_channel_temp = {
                    .fd = -1,
            };
            storage_channel = &storage_channel_temp;

            test_worker_storage_posix_op_fiber_userdata_t user_data = {
                    .read = true,
                    .path = fixture_temp_path,
                    .iovec = iovec,
                    .iovec_nr = 1,
                    .offset = 0,
                    .open_result = &storage_channel,
                    .read_write_result = &len,
            };
            fiber = fiber_scheduler_new_fiber(
                    fiber_name,
                    fiber_name_len,
                    test_worker_storage_posix_op_fiber_entrypoint,
                    &user_data);

            storage_channel = NULL;

            REQUIRE(fiber->error_number == EBADF);
            REQUIRE((int)len == -EBADF);
        }
    }
    SECTION("worker_storage_posix_op_storage_write") {
        SECTION("write n. 1 iovec") {
            size_t len = 0;
            iovec[0].iov_base = buffer_write;
            iovec[0].iov_len = strlen(buffer_write);

            test_worker_storage_posix_op_fiber_userdata_t user_data = {
                    .open = true,
                    .write = true,
                    .path = fixture_temp_path,
                    .open_flags = O_WRONLY,
                    .open_mode = 0,
                    .iovec = iovec,
                    .iovec_nr = 1,
                    .offset = 0,
                    .open_result = &storage_channel,
                    .read_write_result = &len,
            };
            fiber = fiber_scheduler_new_fiber(
                    fiber_name,
                    fiber_name_len,
                    test_worker_storage_posix_op_fiber_entrypoint,
                    &user_data);

            REQUIRE(fiber->error_number == 0);
            REQUIRE(len == strlen(buffer_write));

            int fd = openat(0, fixture_temp_path, O_RDONLY, 0);
            REQUIRE(fd > -1);
            REQUIRE(pread(fd, buffer_read1, strlen(buffer_write), 0) == strlen(buffer_write));
            REQUIRE(strncmp(buffer_write, buffer_read1, strlen(buffer_write)) == 0);
            REQUIRE(close(fd) == 0);
        }

        SECTION("write n. 2 iovec") {
            size_t len = 0;
            iovec[0].iov_base = buffer_write;
            iovec[0].iov_len = strlen(buffer_write);
            iovec[1].iov_base = buffer_write;
            iovec[1].iov_len = strlen(buffer_write);

            test_worker_storage_posix_op_fiber_userdata_t user_data = {
                    .open = true,
                    .write = true,
                    .path = fixture_temp_path,
                    .open_flags = O_WRONLY,
                    .open_mode = 0,
                    .iovec = iovec,
                    .iovec_nr = 2,
                    .offset = 0,
                    .open_result = &storage_channel,
                    .read_write_result = &len,
            };
            fiber = fiber_scheduler_new_fiber(
                    fiber_name,
                    fiber_name_len,
                    test_worker_storage_posix_op_fiber_entrypoint,
                    &user_data);

            REQUIRE(fiber->error_number == 0);
            REQUIRE(len == strlen(buffer_write) * 2);

            int fd = openat(0, fixture_temp_path, O_RDONLY, 0);
            REQUIRE(fd > -1);
            REQUIRE(pread(fd, buffer_read1, strlen(buffer_write), 0) == strlen(buffer_write));
            REQUIRE(pread(fd, buffer_read2, strlen(buffer_write), 0) == strlen(buffer_write));
            REQUIRE(strncmp(buffer_write, buffer_read1, strlen(buffer_write)) == 0);
            REQUIRE(strncmp(buffer_write, buffer_read2, strlen(buffer_write)) == 0);
            REQUIRE(close(fd) == 0);
        }

        SECTION("invalid fd") {
            size_t len = 0;
            iovec[0].iov_base = buffer_read1;
            iovec[0].iov_len = strlen(buffer_write);

            storage_channel_t storage_channel_temp = {
                    .fd = -1,
            };
            storage_channel = &storage_channel_temp;

            test_worker_storage_posix_op_fiber_userdata_t user_data = {
                    .write = true,
                    .path = fixture_temp_path,
                    .iovec = iovec,
                    .iovec_nr = 1,
                    .offset = 0,
                    .open_result = &storage_channel,
                    .read_write_result = &len,
            };
            fiber = fiber_scheduler_new_fiber(
                    fiber_name,
                    fiber_name_len,
                    test_worker_storage_posix_op_fiber_entrypoint,
                    &user_data);

            storage_channel = NULL;

            REQUIRE(fiber->error_number == EBADF);
            REQUIRE((int)len == -EBADF);
        }
    }

    SECTION("worker_storage_posix_op_storage_flush") {
        SECTION("write and flush") {
            bool res = false;
            iovec[0].iov_base = buffer_write;
            iovec[0].iov_len = strlen(buffer_write);

            test_worker_storage_posix_op_fiber_userdata_t user_data = {
                    .open = true,
                    .write = true,
                    .flush = true,
                    .path = fixture_temp_path,
                    .open_flags = O_RDWR,
                    .iovec = iovec,
                    .iovec_nr = 1,
                    .offset = 0,
                    .open_result = &storage_channel,
                    .flush_result = &res,
            };
            fiber = fiber_scheduler_new_fiber(
                    fiber_name,
                    fiber_name_len,
                    test_worker_storage_posix_op_fiber_entrypoint,
                    &user_data);

            REQUIRE(res);
            REQUIRE(fiber->error_number == 0);
        }

        SECTION("invalid fd") {
            bool res = false;
            iovec[0].iov_base = buffer_read1;
            iovec[0].iov_len = strlen(buffer_write);

            storage_channel_t storage_channel_temp = {
                    .fd = -1,
            };
            storage_channel = &storage_channel_temp;

            test_worker_storage_posix_op_fiber_userdata_t user_data = {
                    .flush = true,
                    .open_result = &storage_channel,
                    .flush_result = &res,
            };
            fiber = fiber_scheduler_new_fiber(
                    fiber_name,
                    fiber_name_len,
                    test_worker_storage_posix_op_fiber_entrypoint,
                    &user_data);

            storage_channel = NULL;

            REQUIRE(fiber->error_number == EBADF);
            REQUIRE(res == false);
        }
    }

    SECTION("posix_support_sqe_enqueue_fallocate") {
        SECTION("create and extend to 1kb") {
            struct stat statbuf = { 0 };
            bool res = false;

            test_worker_storage_posix_op_fiber_userdata_t user_data = {
                    .open = true,
                    .fallocate = true,
                    .path = fixture_temp_path,
                    .open_flags = O_RDWR,
                    .fallocate_mode = 0,
                    .fallocate_offset = 0,
                    .fallocate_len = 1024,
                    .open_result = &storage_channel,
                    .fallocate_result = &res,
            };
            fiber = fiber_scheduler_new_fiber(
                    fiber_name,
                    fiber_name_len,
                    test_worker_storage_posix_op_fiber_entrypoint,
                    &user_data);

            REQUIRE(res);
            REQUIRE(fiber->error_number == 0);

            int fd = openat(0, fixture_temp_path, O_RDWR, 0);
            REQUIRE(fd > -1);
            REQUIRE(fstat(fd, &statbuf) == 0);
            REQUIRE(statbuf.st_size == 1024);
            REQUIRE(close(fd) == 0);
        }

        SECTION("create and extend to 1kb - no file size increase") {
            struct stat statbuf = { 0 };
            bool res = false;

            test_worker_storage_posix_op_fiber_userdata_t user_data = {
                    .open = true,
                    .fallocate = true,
                    .path = fixture_temp_path,
                    .open_flags = O_RDWR,
                    .fallocate_mode = FALLOC_FL_KEEP_SIZE,
                    .fallocate_offset = 0,
                    .fallocate_len = 1024,
                    .open_result = &storage_channel,
                    .fallocate_result = &res,
            };
            fiber = fiber_scheduler_new_fiber(
                    fiber_name,
                    fiber_name_len,
                    test_worker_storage_posix_op_fiber_entrypoint,
                    &user_data);

            REQUIRE(res);
            REQUIRE(fiber->error_number == 0);

            int fd = openat(0, fixture_temp_path, O_RDWR, 0);
            REQUIRE(fd > -1);
            REQUIRE(fstat(fd, &statbuf) == 0);
            REQUIRE(statbuf.st_size == 0);
            REQUIRE(close(fd) == 0);
        }

        SECTION("invalid fd") {
            bool res = false;
            iovec[0].iov_base = buffer_read1;
            iovec[0].iov_len = strlen(buffer_write);

            storage_channel_t storage_channel_temp = {
                    .fd = -1,
            };
            storage_channel = &storage_channel_temp;

            test_worker_storage_posix_op_fiber_userdata_t user_data = {
                    .fallocate = true,
                    .fallocate_mode = 0,
                    .fallocate_offset = 0,
                    .fallocate_len = 1024,
                    .open_result = &storage_channel,
                    .fallocate_result = &res,
            };
            fiber = fiber_scheduler_new_fiber(
                    fiber_name,
                    fiber_name_len,
                    test_worker_storage_posix_op_fiber_entrypoint,
                    &user_data);

            storage_channel = NULL;

            REQUIRE(fiber->error_number == EBADF);
            REQUIRE(res == false);
        }
    }

    SECTION("worker_storage_posix_op_storage_close") {
        SECTION("close a channel") {
            SECTION("open an existing file") {
                test_worker_storage_posix_op_fiber_userdata_t user_data = {
                        .open = true,
                        .path = fixture_temp_path,
                        .open_flags = O_RDONLY,
                        .open_mode = 0,
                        .open_result = &storage_channel
                };
                fiber = fiber_scheduler_new_fiber(
                        fiber_name,
                        fiber_name_len,
                        test_worker_storage_posix_op_fiber_entrypoint,
                        &user_data);

                REQUIRE(worker_storage_posix_op_storage_close(storage_channel));

                storage_channel = NULL;
            }
        }
    }

    SECTION("worker_storage_posix_initialize") {
        worker_context_t worker_context = { 0 };

        REQUIRE(worker_storage_posix_initialize(&worker_context));
    }

    SECTION("worker_storage_posix_cleanup") {
        worker_context_t worker_context = { 0 };

        REQUIRE(worker_storage_posix_cleanup(&worker_context));
    }

    SECTION("worker_storage_posix_op_register") {
        worker_storage_posix_op_register();

        REQUIRE(worker_op_storage_open == worker_storage_posix_op_storage_open);
        REQUIRE(worker_op_storage_read == worker_storage_posix_op_storage_read);
        REQUIRE(worker_op_storage_write == worker_storage_posix_op_storage_write);
        REQUIRE(worker_op_storage_flush == worker_storage_posix_op_storage_flush);
        REQUIRE(worker_op_storage_fallocate == worker_storage_posix_op_storage_fallocate);
        REQUIRE(worker_op_storage_close == worker_storage_posix_op_storage_close);
    }

    if (fiber) {
        fiber_free(fiber);
    }

    if (storage_channel && storage_channel->fd != -1) {
        storage_io_common_close(storage_channel->fd);

        storage_channel_free(storage_channel);
    }

    unlink(fixture_temp_path);
}
