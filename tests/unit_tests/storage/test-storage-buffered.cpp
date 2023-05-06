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
#include "storage/channel/storage_buffered_channel.h"

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

#include "storage/storage_buffered.h"

extern thread_local fiber_scheduler_stack_t fiber_scheduler_stack;

TEST_CASE("storage/storage_buffered.c", "[storage][storage_buffered]") {
    char *fixture_temp_path_copy;
    storage_channel_t *storage_channel;
    storage_buffered_channel_t *storage_buffered_channel;

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

    worker_storage_posix_op_register();

    storage_channel = storage_open(
            fixture_temp_path_copy,
            O_RDWR,
            0);
    storage_buffered_channel = storage_buffered_channel_new(storage_channel);

    SECTION("storage_buffered_get_offset") {
        SECTION("Valid offset value") {
            storage_buffered_channel->offset = 10;
            REQUIRE(storage_buffered_get_offset(storage_buffered_channel) == 10);
        }
    }

    SECTION("storage_buffered_set_offset") {
        SECTION("Setting offset value") {
            storage_buffered_set_offset(storage_buffered_channel, 20);
            REQUIRE(storage_buffered_channel->offset == 20);
        }
    }

    SECTION("storage_buffered_read_ahead") {
        SECTION("Without buffer window rollback") {
            int fd = openat(0, fixture_temp_path, O_WRONLY, 0);
            REQUIRE(fd > -1);
            REQUIRE(write(fd, buffer_write, strlen(buffer_write)) == strlen(buffer_write));
            REQUIRE(close(fd) == 0);

            REQUIRE(storage_buffered_read_ahead(storage_buffered_channel, strlen(buffer_write)) > 0);
            REQUIRE(storage_buffered_channel->offset == strlen(buffer_write));
            REQUIRE(storage_buffered_channel->buffers.read.buffer.data_size == strlen(buffer_write));
            REQUIRE(storage_buffered_channel->buffers.read.buffer.data_offset == 0);
            REQUIRE(memcmp(storage_buffered_channel->buffers.read.buffer.data, buffer_write, strlen(buffer_write)) == 0);
        }


        SECTION("Without buffer window rollback") {
            uint64_t expected_value;

            int fd = openat(0, fixture_temp_path, O_WRONLY, 0);
            REQUIRE(fd > -1);
            for(uint64_t i = 0; i < (storage_buffered_channel->buffers.read.buffer.length * 2) / sizeof(i); i++) {
                REQUIRE(write(fd, (char*)&i, sizeof(i)) == sizeof(i));
            }
            REQUIRE(close(fd) == 0);

            // Fill the buffer
            REQUIRE(storage_buffered_read_ahead(storage_buffered_channel, storage_buffered_channel->buffers.read.buffer.length));
            REQUIRE(storage_buffered_channel->buffers.read.buffer.data_size == storage_buffered_channel->buffers.read.buffer.length);

            // Simulate reading up to almost the end of the buffer
            storage_buffered_channel->buffers.read.buffer.data_offset = storage_buffered_channel->buffers.read.buffer.length - 256;

            // Try to read ahead
            REQUIRE(storage_buffered_read_ahead(storage_buffered_channel, 8192) == 8192 + (STORAGE_BUFFERED_PAGE_SIZE * 2));

            // The buffer should now contain 14kb of data plus the 256 bytes we read before
            REQUIRE(storage_buffered_channel->buffers.read.buffer.data_size == 8192 + (STORAGE_BUFFERED_PAGE_SIZE * 2) + 256);
            REQUIRE(storage_buffered_channel->buffers.read.buffer.data_offset == 0);

            // Calculate which should be the value written at the beginning of the buffer
            expected_value = (storage_buffered_channel->buffers.read.buffer.length - 256) / sizeof(uint64_t);
            REQUIRE(memcmp(storage_buffered_channel->buffers.read.buffer.data, (char*)&expected_value, sizeof(expected_value)) == 0);

            // Calculate which should be the value written at the end of the buffer
            expected_value = expected_value + ((256 + 8192 + (STORAGE_BUFFERED_PAGE_SIZE * 2)) / sizeof(uint64_t)) - 1;
            uint64_t found_value = *(uint64_t*)(
                    storage_buffered_channel->buffers.read.buffer.data +
                    storage_buffered_channel->buffers.read.buffer.data_size -
                    sizeof(uint64_t));
            REQUIRE(expected_value == found_value);
        }
    }

    SECTION("storage_buffered_flush_write") {
        memcpy(storage_buffered_channel->buffers.write.buffer.data, buffer_write, strlen(buffer_write));
        storage_buffered_channel->buffers.write.buffer.data_size = strlen(buffer_write);

        SECTION("Successful write") {
            REQUIRE(storage_buffered_flush_write(storage_buffered_channel) == true);
            REQUIRE(storage_buffered_channel->offset == strlen(buffer_write));
            REQUIRE(storage_buffered_channel->buffers.write.buffer.data_size == 0);
            REQUIRE(storage_buffered_channel->buffers.write.buffer.data_offset == 0);
        }

        SECTION("Failed write") {
            // Change the file descriptor of the storage channel to make the write fail
            storage_io_common_fd_t original_fd = storage_buffered_channel->storage_channel->fd;
            storage_buffered_channel->storage_channel->fd = 01;

            // Try to write but do not check the result, the original file descriptor needs to be restored before
            // invoking REQUIRE to avoid leaving any dangling file descriptor
            bool result = storage_buffered_flush_write(storage_buffered_channel);

            // Restore the original file descriptor
            storage_buffered_channel->storage_channel->fd = original_fd;

            // Check the result
            REQUIRE(result == false);
            REQUIRE(storage_buffered_channel->offset == 0);
            REQUIRE(storage_buffered_channel->buffers.write.buffer.data_size == strlen(buffer_write));
            REQUIRE(storage_buffered_channel->buffers.write.buffer.data_offset == 0);
        }
    }

    SECTION("storage_buffered_read_buffer_acquire_slice") {
        memcpy(storage_buffered_channel->buffers.read.buffer.data, buffer_write, strlen(buffer_write));
        storage_buffered_channel->buffers.read.buffer.data_size = strlen(buffer_write);

        SECTION("Read less data than available") {
            storage_buffered_channel_buffer_data_t *buffer_out;
            size_t slice_length = 10;
            size_t acquired_length = storage_buffered_read_buffer_acquire_slice(
                    storage_buffered_channel,
                    slice_length,
                    &buffer_out);
            REQUIRE(acquired_length == slice_length);
            REQUIRE(buffer_out != NULL);
            REQUIRE(memcmp(buffer_out, buffer_write, acquired_length) == 0);
        }

        SECTION("Read more data than available") {
            storage_buffered_channel_buffer_data_t *buffer_out;
            size_t slice_length = 10000;
            size_t acquired_length = storage_buffered_read_buffer_acquire_slice(
                    storage_buffered_channel,
                    slice_length,
                    &buffer_out);
            REQUIRE(acquired_length <= slice_length);
            REQUIRE(buffer_out != NULL);
            REQUIRE(memcmp(buffer_out, buffer_write, acquired_length) == 0);
        }
    }

    SECTION("storage_buffered_write_buffer_acquire_slice") {
        SECTION("Acquire a slice") {
            size_t data_size = strlen(buffer_write) + 1;
            size_t slice_length = data_size + 10;
            auto buffer_out = storage_buffered_write_buffer_acquire_slice(
                    storage_buffered_channel,
                    slice_length);

            REQUIRE(buffer_out != NULL);

            strncpy(buffer_out, buffer_write, data_size);
            REQUIRE(strncmp(storage_buffered_channel->buffers.write.buffer.data, buffer_write, data_size) == 0);
        }
    }

    SECTION("storage_buffered_write_buffer_release_slice") {
        SECTION("Acquire a slice") {
            size_t data_size = strlen(buffer_write) + 1;
            size_t slice_length = data_size + 10;
            auto buffer_out = storage_buffered_write_buffer_acquire_slice(
                    storage_buffered_channel,
                    slice_length);

            REQUIRE(buffer_out != NULL);

            strncpy(buffer_out, buffer_write, data_size);
            storage_buffered_write_buffer_release_slice(storage_buffered_channel, data_size);

            REQUIRE(strncmp(storage_buffered_channel->buffers.write.buffer.data, buffer_write, data_size) == 0);
            REQUIRE(storage_buffered_channel->offset == 0);
            REQUIRE(storage_buffered_channel->buffers.write.buffer.data_size == data_size);
            REQUIRE(storage_buffered_channel->buffers.write.buffer.data_offset == data_size);
        }
    }

    storage_io_common_close(storage_channel->fd);
    storage_channel_free(storage_channel);
    storage_buffered_channel_free(storage_buffered_channel);
    unlink(fixture_temp_path);

    xalloc_free(fiber_scheduler_stack.list);
    fiber_scheduler_stack.list = nullptr;
    fiber_scheduler_stack.index = -1;
    fiber_scheduler_stack.size = 0;
}
