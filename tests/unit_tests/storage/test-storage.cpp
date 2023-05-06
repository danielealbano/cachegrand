/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"

#include "misc.h"
#include "exttypes.h"
#include "xalloc.h"
#include "fiber/fiber.h"
#include "fiber/fiber_scheduler.h"
#include "config.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/storage/worker_storage_posix_op.h"
#include "memory_allocator/ffma.h"

#include "storage/storage.h"

extern thread_local fiber_scheduler_stack_t fiber_scheduler_stack;

TEST_CASE("storage/storage.c", "[storage][storage]") {
    char *fixture_temp_path_copy;
    storage_channel_t *storage_channel = nullptr;

    char fiber_name[] = "test-fiber";
    fiber_t fiber = {
            .name = fiber_name,
    };

    worker_context_t worker_context = { 0 };
    worker_context_set(&worker_context);

    if (!fiber_scheduler_stack.list) {
        fiber_scheduler_grow_stack();
    }
    fiber_scheduler_stack.list[0] = &fiber;
    fiber_scheduler_stack.index = 0;

    char fixture_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
    int fixture_temp_path_suffix_len = 4;
    close(mkstemps(fixture_temp_path, fixture_temp_path_suffix_len));
    fixture_temp_path_copy = (char *)(ffma_mem_alloc(strlen(fixture_temp_path) + 1));
    strcpy(fixture_temp_path_copy, fixture_temp_path);

    char buffer_write[] = "cachegrand test - read / write tests";
    char buffer_read1[128] = { 0 }, buffer_read2[128] = { 0 };
    struct iovec iovec[2] = { nullptr };

    worker_storage_posix_op_register();

    SECTION("storage_open") {
        SECTION("open an existing file") {
            storage_channel = storage_open(
                    fixture_temp_path_copy,
                    O_RDONLY,
                    0);

            REQUIRE(storage_channel != nullptr);
            REQUIRE(fiber.error_number == 0);
            REQUIRE(storage_channel->fd > -1);
            REQUIRE(strncmp(storage_channel->path, fixture_temp_path, strlen(fixture_temp_path)) == 0);
            REQUIRE(strlen(fixture_temp_path) == storage_channel->path_len);

            REQUIRE(worker_context.stats.internal.storage.open_files == 1);
        }

        SECTION("open a non-existing file creating it") {
            // The file gets pre-created for convenience during the test setup, need to be unlinked for the test to
            // be able to reuse the unique file name
            unlink(fixture_temp_path);
            storage_channel = storage_open(
                    fixture_temp_path_copy,
                    O_CREAT | O_RDWR | O_EXCL,
                    S_IRUSR | S_IWUSR);

            REQUIRE(storage_channel != nullptr);
            REQUIRE(fiber.error_number == 0);
            REQUIRE(storage_channel->fd > -1);
            REQUIRE(strncmp(storage_channel->path, fixture_temp_path, strlen(fixture_temp_path)) == 0);
            REQUIRE(strlen(fixture_temp_path) == storage_channel->path_len);

            REQUIRE(worker_context.stats.internal.storage.open_files == 1);
        }

        SECTION("fail to open an non-existing file without create option") {
            // The file gets pre-created for convenience during the test setup, need to be unlinked for the test to
            // be able to reuse the unique file name
            unlink(fixture_temp_path);
            storage_channel = storage_open(
                    fixture_temp_path_copy,
                    O_RDONLY,
                    0);

            REQUIRE(storage_channel == nullptr);
            REQUIRE(fiber.error_number == ENOENT);
            REQUIRE(storage_channel == nullptr);

            REQUIRE(worker_context.stats.internal.storage.open_files == 0);
        }
    }

    SECTION("storage_readv") {
        SECTION("read n. 1 iovec") {
            iovec[0].iov_base = buffer_read1;
            iovec[0].iov_len = strlen(buffer_write);

            int fd = openat(0, fixture_temp_path, O_WRONLY, 0);
            REQUIRE(fd > -1);
            REQUIRE(write(fd, buffer_write, strlen(buffer_write)) == strlen(buffer_write));
            REQUIRE(close(fd) == 0);

            storage_channel = storage_open(fixture_temp_path_copy, O_RDONLY, 0);

            REQUIRE(storage_channel != nullptr);
            REQUIRE(storage_readv(storage_channel, iovec, 1, iovec[0].iov_len, 0));
            REQUIRE(fiber.error_number == 0);
            REQUIRE(strncmp(buffer_write, buffer_read1, strlen(buffer_write)) == 0);
            REQUIRE(worker_context.stats.internal.storage.read_data == iovec[0].iov_len);
            REQUIRE(worker_context.stats.internal.storage.read_iops == 1);
        }

        SECTION("read n. 2 iovec") {
            iovec[0].iov_base = buffer_read1;
            iovec[0].iov_len = strlen(buffer_write);
            iovec[1].iov_base = buffer_read2;
            iovec[1].iov_len = strlen(buffer_write);

            int fd = openat(0, fixture_temp_path, O_WRONLY, 0);
            REQUIRE(fd > -1);
            REQUIRE(write(fd, buffer_write, strlen(buffer_write)) == strlen(buffer_write));
            REQUIRE(write(fd, buffer_write, strlen(buffer_write)) == strlen(buffer_write));
            REQUIRE(close(fd) == 0);

            storage_channel = storage_open(fixture_temp_path_copy, O_RDONLY, 0);

            REQUIRE(storage_channel != nullptr);
            REQUIRE(storage_readv(storage_channel, iovec, 2, iovec[0].iov_len + iovec[1].iov_len, 0));
            REQUIRE(fiber.error_number == 0);
            REQUIRE(strncmp(buffer_write, buffer_read1, strlen(buffer_write)) == 0);
            REQUIRE(strncmp(buffer_write, buffer_read2, strlen(buffer_write)) == 0);
            REQUIRE(worker_context.stats.internal.storage.read_data == iovec[0].iov_len + iovec[1].iov_len);
            REQUIRE(worker_context.stats.internal.storage.read_iops == 1);
        }

        SECTION("invalid fd") {
            iovec[0].iov_base = buffer_read1;
            iovec[0].iov_len = strlen(buffer_write);

            storage_channel_t storage_channel_temp = {
                    .fd = -1,
            };
            storage_channel = &storage_channel_temp;

            REQUIRE(storage_readv(storage_channel, iovec, 1, iovec[0].iov_len, 0) == false);
            REQUIRE(fiber.error_number == EBADF);
            REQUIRE(worker_context.stats.internal.storage.read_data == 0);
            REQUIRE(worker_context.stats.internal.storage.read_iops == 0);

            storage_channel = nullptr;
        }
    }

    SECTION("storage_read") {
        SECTION("read buffer") {
            int fd = openat(0, fixture_temp_path, O_WRONLY, 0);
            REQUIRE(fd > -1);
            REQUIRE(write(fd, buffer_write, strlen(buffer_write)) == strlen(buffer_write));
            REQUIRE(close(fd) == 0);

            storage_channel = storage_open(fixture_temp_path_copy, O_RDONLY, 0);

            REQUIRE(storage_channel != nullptr);
            REQUIRE(storage_read(storage_channel, buffer_read1, strlen(buffer_write), 0));
            REQUIRE(fiber.error_number == 0);
            REQUIRE(strncmp(buffer_write, buffer_read1, strlen(buffer_write)) == 0);
            REQUIRE(worker_context.stats.internal.storage.read_data == strlen(buffer_write));
            REQUIRE(worker_context.stats.internal.storage.read_iops == 1);
        }

        SECTION("invalid fd") {
            storage_channel_t storage_channel_temp = {
                    .fd = -1,
            };
            storage_channel = &storage_channel_temp;

            REQUIRE(storage_read(storage_channel, buffer_read1, strlen(buffer_write), 0) == false);
            REQUIRE(fiber.error_number == EBADF);
            REQUIRE(worker_context.stats.internal.storage.read_data == 0);
            REQUIRE(worker_context.stats.internal.storage.read_iops == 0);

            storage_channel = nullptr;
        }
    }

    SECTION("storage_writev") {
        SECTION("write n. 1 iovec") {
            iovec[0].iov_base = buffer_write;
            iovec[0].iov_len = strlen(buffer_write);

            storage_channel = storage_open(fixture_temp_path_copy, O_WRONLY, 0);

            REQUIRE(storage_channel != nullptr);
            REQUIRE(storage_writev(storage_channel, iovec, 1, iovec[0].iov_len, 0));
            REQUIRE(fiber.error_number == 0);

            int fd = openat(0, fixture_temp_path, O_RDONLY, 0);
            REQUIRE(fd > -1);
            REQUIRE(pread(fd, buffer_read1, strlen(buffer_write), 0) == strlen(buffer_write));
            REQUIRE(strncmp(buffer_write, buffer_read1, strlen(buffer_write)) == 0);
            REQUIRE(close(fd) == 0);
            REQUIRE(worker_context.stats.internal.storage.written_data == iovec[0].iov_len);
            REQUIRE(worker_context.stats.internal.storage.write_iops == 1);
        }

        SECTION("write n. 2 iovec") {
            iovec[0].iov_base = buffer_write;
            iovec[0].iov_len = strlen(buffer_write);
            iovec[1].iov_base = buffer_write;
            iovec[1].iov_len = strlen(buffer_write);

            storage_channel = storage_open(fixture_temp_path_copy, O_WRONLY, 0);

            REQUIRE(storage_channel != nullptr);
            REQUIRE(storage_writev(storage_channel, iovec, 2, iovec[0].iov_len + iovec[1].iov_len, 0));
            REQUIRE(fiber.error_number == 0);

            int fd = openat(0, fixture_temp_path, O_RDONLY, 0);
            REQUIRE(fd > -1);
            REQUIRE(pread(fd, buffer_read1, strlen(buffer_write), 0) == strlen(buffer_write));
            REQUIRE(pread(fd, buffer_read2, strlen(buffer_write), 0) == strlen(buffer_write));
            REQUIRE(strncmp(buffer_write, buffer_read1, strlen(buffer_write)) == 0);
            REQUIRE(strncmp(buffer_write, buffer_read2, strlen(buffer_write)) == 0);
            REQUIRE(close(fd) == 0);
            REQUIRE(worker_context.stats.internal.storage.written_data == iovec[0].iov_len + iovec[1].iov_len);
            REQUIRE(worker_context.stats.internal.storage.write_iops == 1);
        }

        SECTION("invalid fd") {
            iovec[0].iov_base = buffer_read1;
            iovec[0].iov_len = strlen(buffer_write);

            storage_channel_t storage_channel_temp = {
                    .fd = -1,
            };
            storage_channel = &storage_channel_temp;

            REQUIRE(storage_writev(storage_channel, iovec, 1, iovec[0].iov_len, 0) == false);
            REQUIRE(fiber.error_number == EBADF);
            REQUIRE(worker_context.stats.internal.storage.written_data == 0);
            REQUIRE(worker_context.stats.internal.storage.write_iops == 0);

            storage_channel = nullptr;
        }
    }

    SECTION("storage_write") {
        SECTION("write buffer") {
            storage_channel = storage_open(fixture_temp_path_copy, O_WRONLY, 0);

            REQUIRE(storage_channel != nullptr);
            REQUIRE(storage_write(storage_channel, buffer_write, strlen(buffer_write), 0));
            REQUIRE(fiber.error_number == 0);

            int fd = openat(0, fixture_temp_path, O_RDONLY, 0);
            REQUIRE(fd > -1);
            REQUIRE(pread(fd, buffer_read1, strlen(buffer_write), 0) == strlen(buffer_write));
            REQUIRE(strncmp(buffer_write, buffer_read1, strlen(buffer_write)) == 0);
            REQUIRE(close(fd) == 0);
            REQUIRE(worker_context.stats.internal.storage.written_data == strlen(buffer_write));
            REQUIRE(worker_context.stats.internal.storage.write_iops == 1);
        }

        SECTION("invalid fd") {
            storage_channel_t storage_channel_temp = {
                    .fd = -1,
            };
            storage_channel = &storage_channel_temp;

            REQUIRE(storage_write(storage_channel, buffer_write, strlen(buffer_write), 0) == false);
            REQUIRE(fiber.error_number == EBADF);
            REQUIRE(worker_context.stats.internal.storage.written_data == 0);
            REQUIRE(worker_context.stats.internal.storage.write_iops == 0);

            storage_channel = nullptr;
        }
    }

    SECTION("storage_flush") {
        SECTION("valid fd") {
            storage_channel = storage_open(fixture_temp_path_copy, O_WRONLY, 0);

            REQUIRE(storage_channel != nullptr);
            REQUIRE(storage_flush(storage_channel));
            REQUIRE(fiber.error_number == 0);
        }

        SECTION("invalid fd") {
            storage_channel_t storage_channel_temp = {
                    .fd = -1,
            };
            storage_channel = &storage_channel_temp;

            REQUIRE(storage_flush(storage_channel) == false);
            REQUIRE(fiber.error_number == EBADF);

            storage_channel = nullptr;
        }
    }

    SECTION("storage_fallocate") {
        SECTION("create and extend to 1kb") {
            struct stat statbuf = { 0 };

            storage_channel = storage_open(fixture_temp_path_copy, O_WRONLY, 0);
            REQUIRE(storage_channel != nullptr);
            REQUIRE(storage_fallocate(storage_channel, 0, 0, 1024));
            REQUIRE(fiber.error_number == 0);

            int fd = openat(0, fixture_temp_path, O_RDWR, 0);
            REQUIRE(fd > -1);
            REQUIRE(fstat(fd, &statbuf) == 0);
            REQUIRE(statbuf.st_size == 1024);
            REQUIRE(close(fd) == 0);
        }

        SECTION("create and extend to 1kb - no file size increase") {
            struct stat statbuf = { 0 };

            storage_channel = storage_open(fixture_temp_path_copy, O_WRONLY, 0);
            REQUIRE(storage_channel != nullptr);
            REQUIRE(storage_fallocate(storage_channel, FALLOC_FL_KEEP_SIZE, 0, 1024));
            REQUIRE(fiber.error_number == 0);

            int fd = openat(0, fixture_temp_path, O_RDWR, 0);
            REQUIRE(fd > -1);
            REQUIRE(fstat(fd, &statbuf) == 0);
            REQUIRE(statbuf.st_size == 0);
            REQUIRE(close(fd) == 0);
        }

        SECTION("invalid fd") {
            storage_channel_t storage_channel_temp = {
                    .fd = -1,
            };
            storage_channel = &storage_channel_temp;

            REQUIRE(storage_fallocate(storage_channel, 0, 0, 1024) == false);
            REQUIRE(fiber.error_number == EBADF);
        }
    }

    SECTION("storage_close") {
        SECTION("valid fd") {
            storage_channel = storage_open(fixture_temp_path_copy, O_RDONLY, 0);

            REQUIRE(storage_channel != nullptr);
            REQUIRE(storage_close(storage_channel));
            REQUIRE(fiber.error_number == 0);

            REQUIRE(worker_context.stats.internal.storage.open_files == 0);
        }

        SECTION("invalid fd") {
            storage_channel_t storage_channel_temp = {
                    .fd = -1,
            };
            storage_channel = &storage_channel_temp;

            REQUIRE(storage_close(storage_channel) == false);
            REQUIRE(fiber.error_number == EBADF);

            storage_channel = nullptr;
        }
    }

    if (storage_channel && storage_channel->fd != -1) {
        storage_io_common_close(storage_channel->fd);
        storage_channel_free(storage_channel);
    } else {
        ffma_mem_free(fixture_temp_path_copy);
    }

    unlink(fixture_temp_path);

    xalloc_free(fiber_scheduler_stack.list);
    fiber_scheduler_stack.list = nullptr;
    fiber_scheduler_stack.index = -1;
    fiber_scheduler_stack.size = 0;
}
