/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <arpa/inet.h>
#include <liblzf/lzf.h>

#pragma GCC diagnostic ignored "-Wwrite-strings"

#include "random.h"

#include "module/redis/snapshot/module_redis_snapshot.h"
#include "module/redis/snapshot/module_redis_snapshot_serialize_primitive.h"

bool test_module_redis_snapshot_serialize_primitive_redis_check_rdb_available() {
    char command[256];

    snprintf(command, sizeof(command), "which %s > /dev/null", "redis-check-rdb");

    return system(command) == 0;
}

bool test_module_redis_snapshot_serialize_primitive_vaidate_rdb(
        char *buffer,
        size_t buffer_length) {
    char temp_test_snapshot_fd_path[255];
    char temp_test_snapshot_path[255];
    char temp_test_snapshot_template[] =  "/tmp/cachegrand-tests-XXXXXX";

    // Generate a temporary file
    int temp_test_snapshot_fd = mkstemp(temp_test_snapshot_template);

    // Get the path to the file descriptor
    sprintf(temp_test_snapshot_fd_path, "/proc/self/fd/%d", temp_test_snapshot_fd);
    REQUIRE(readlink(
            temp_test_snapshot_fd_path,
            temp_test_snapshot_path,
            sizeof(temp_test_snapshot_path)) != -1);

    // Write the buffer to the disk
    size_t written_data = 0;
    while (written_data < buffer_length) {
        ssize_t written_data_chunk = write(
                temp_test_snapshot_fd,
                buffer + written_data,
                buffer_length - written_data);
        REQUIRE(written_data_chunk != -1);
        written_data += written_data_chunk;
    }

    // Sync and close the file descriptor
    fsync(temp_test_snapshot_fd);
    close(temp_test_snapshot_fd);

    // Run redis-check-rdb on the file to ensure it can be read, if the operation fails print out the command output
    char command[1024];
    char command_temp_buffer[1024];
    char command_output[64 * 1024];
    char *command_output_ptr = command_output;
    size_t command_output_offset = 0;

    // Build the command
    sprintf(command, "redis-check-rdb %s 2>&1", temp_test_snapshot_path);

    // Open the command for reading
    FILE* fp = popen(command, "r");
    REQUIRE(fp != NULL);

    // Read the output of the command
    while (fgets(command_temp_buffer, sizeof(command_temp_buffer), fp) != nullptr) {
        size_t command_temp_buffer_length = strlen(command_temp_buffer);

        REQUIRE(command_output_offset + command_temp_buffer_length < sizeof(command_output));

        memcpy(command_output_ptr, command_temp_buffer, command_temp_buffer_length);
        command_output_ptr += command_temp_buffer_length;
        command_output_offset += command_temp_buffer_length;
    }

    // Close the pipe and get the exit status of the command
    int exit_status = pclose(fp);
    REQUIRE(exit_status != -1);

    // If the test failed print out the command output
    if (exit_status != 0) {
        fprintf(stderr, "Command: %s\n", command);
        fprintf(stderr, "Output:\n%s\n", command_output);
        fflush(stderr);
    }

    // Remove the file before checking if the test failed
    unlink(temp_test_snapshot_path);

    return exit_status == 0;
}

TEST_CASE("module_redis_snapshot_serialize_primitive") {
    SECTION("module_redis_snapshot_serialize_primitive_encode_length_required_buffer_space") {
        SECTION("length <= 63") {
            size_t result =
                    module_redis_snapshot_serialize_primitive_encode_length_required_buffer_space(63);
            REQUIRE(result == 1);
        }

        SECTION("length <= 16383") {
            size_t result =
                    module_redis_snapshot_serialize_primitive_encode_length_required_buffer_space(16383);
            REQUIRE(result == 2);
        }

        SECTION("length <= UINT32_MAX") {
            size_t result =
                    module_redis_snapshot_serialize_primitive_encode_length_required_buffer_space(UINT32_MAX);
            REQUIRE(result == 4);
        }

        SECTION("length > UINT32_MAX") {
            size_t result =
                    module_redis_snapshot_serialize_primitive_encode_length_required_buffer_space(UINT64_MAX);
            REQUIRE(result == 8);
        }
    }

    SECTION("module_redis_snapshot_serialize_primitive_can_encode_string_int") {
        SECTION("negative signed integer") {
            int64_t output;
            bool result =
                    module_redis_snapshot_serialize_primitive_can_encode_string_int(
                            "-2147483648",
                            11,
                            &output);
            REQUIRE(result == true);
            REQUIRE(output == INT32_MIN);
        }

        SECTION("positive signed integer") {
            int64_t output;
            bool result =
                    module_redis_snapshot_serialize_primitive_can_encode_string_int(
                            "2147483647",
                            10,
                            &output);
            REQUIRE(result == true);
            REQUIRE(output == INT32_MAX);
        }

        SECTION("invalid string") {
            int64_t output;
            bool result =
                    module_redis_snapshot_serialize_primitive_can_encode_string_int(
                            "invalid",
                            7,
                            &output);
            REQUIRE(result == false);
        }

        SECTION("value > INT32_MAX") {
            int64_t output;
            bool result =
                    module_redis_snapshot_serialize_primitive_can_encode_string_int(
                            "2147483648",
                            10,
                            &output);
            REQUIRE(result == false);
        }

        SECTION("value < INT32_MIN") {
            int64_t output;
            bool result =
                    module_redis_snapshot_serialize_primitive_can_encode_string_int(
                            "-2147483649",
                            11,
                            &output);
            REQUIRE(result == false);
        }
    }

    SECTION("module_redis_snapshot_serialize_primitive_encode_header") {
        SECTION("normal case") {
            module_redis_snapshot_header_t header = {.version = 6};
            uint8_t buffer[9];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_header(
                            &header,
                            buffer,
                            buffer_size,
                            buffer_offset,
                            &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 9);
            REQUIRE(strncmp((const char *) buffer, "REDIS", 5) == 0);
            REQUIRE(strncmp((const char *) (buffer + 5), "0006", 4) == 0);
        }

        SECTION("buffer overflow") {
            module_redis_snapshot_header_t header = {.version = 6};
            uint8_t buffer[8];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_header(
                            &header,
                            buffer,
                            buffer_size,
                            buffer_offset,
                            &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW);
        }

        SECTION("buffer offset") {
            module_redis_snapshot_header_t header = { .version = 6 };
            uint8_t buffer[2 + 9];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 2;
            size_t buffer_offset_out;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_header(
                            &header,
                            buffer,
                            buffer_size,
                            buffer_offset,
                            &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 11);
            REQUIRE(strncmp((const char*)(buffer + 2), "REDIS", 5) == 0);
            REQUIRE(strncmp((const char*)(buffer + 7), "0006", 4) == 0);
        }
    }

    SECTION("module_redis_snapshot_serialize_primitive_encode_length") {
        SECTION("encoding numbers <= 63") {
            uint64_t length = 42;
            uint8_t buffer[1];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_length(
                            length,
                            buffer,
                            buffer_size,
                            buffer_offset,
                            &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 1);
            REQUIRE(buffer[0] == 42);
        }

        SECTION("encoding numbers <= 16383") {
            uint64_t length = 8192;
            uint8_t buffer[2];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_length(
                            length,
                            buffer,
                            buffer_size,
                            buffer_offset,
                            &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 2);
            REQUIRE(buffer[0] == (0x40 | (length >> 8)));
            REQUIRE(buffer[1] == 0x00);
        }

        SECTION("encoding numbers <= UINT32_MAX") {
            uint64_t length = UINT32_MAX;
            uint8_t buffer[5];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_length(
                        length,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 5);
            REQUIRE(buffer[0] == 0x80);
            REQUIRE(buffer[1] == 0xFF);
            REQUIRE(buffer[2] == 0xFF);
            REQUIRE(buffer[3] == 0xFF);
            REQUIRE(buffer[4] == 0xFF);
        }

        SECTION("encoding numbers > UINT32_MAX") {
            uint64_t length = UINT64_MAX;
            uint8_t buffer[9];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_length(
                        length,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 9);
            REQUIRE(buffer[0] == 0x81);
            REQUIRE(buffer[1] == 0xFF);
            REQUIRE(buffer[2] == 0xFF);
            REQUIRE(buffer[3] == 0xFF);
            REQUIRE(buffer[4] == 0xFF);
            REQUIRE(buffer[5] == 0xFF);
            REQUIRE(buffer[6] == 0xFF);
            REQUIRE(buffer[7] == 0xFF);
            REQUIRE(buffer[8] == 0xFF);
        }
    }

    SECTION("module_redis_snapshot_serialize_primitive_encode_opcode_db_number") {
        SECTION("normal case") {
            uint64_t db_number = 3;
            uint8_t buffer[4];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_opcode_db_number(
                        db_number,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 2);
            REQUIRE(buffer[0] == MODULE_REDIS_SNAPSHOT_OPCODE_DB_NUMBER);
            REQUIRE(buffer[1] == db_number);
        }

        SECTION("buffer overflow") {
            uint64_t db_number = 3;
            uint8_t buffer[1];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_opcode_db_number(
                        db_number,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW);
        }

        SECTION("buffer offset") {
            uint64_t db_number = 3;
            uint8_t buffer[4];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 1;
            size_t buffer_offset_out;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_opcode_db_number(
                        db_number,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 3);
            REQUIRE(buffer[1] == MODULE_REDIS_SNAPSHOT_OPCODE_DB_NUMBER);
            REQUIRE(buffer[2] == db_number);
        }
    }

    SECTION("module_redis_snapshot_serialize_primitive_encode_opcode_expire_time_s") {
        SECTION("normal case") {
            uint32_t expire_time_s = 123;
            uint8_t buffer[6];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_opcode_expire_time_s(
                        expire_time_s,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 5);
            REQUIRE(buffer[0] == MODULE_REDIS_SNAPSHOT_OPCODE_EXPIRE_TIME);
            REQUIRE(int32_letoh(*((uint32_t *) (buffer + 1))) == expire_time_s);
        }

        SECTION("buffer overflow") {
            uint32_t expire_time_s = 123;
            uint8_t buffer[4];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_opcode_expire_time_s(
                        expire_time_s,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW);
        }
    }

    SECTION("module_redis_snapshot_serialize_primitive_encode_opcode_expire_time_ms") {
        SECTION("normal case") {
            uint64_t expire_time_ms = 123456;
            uint8_t buffer[9];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_opcode_expire_time_ms(
                        expire_time_ms,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 9);
            REQUIRE(buffer[0] == MODULE_REDIS_SNAPSHOT_OPCODE_EXPIRE_TIME_MS);
            REQUIRE(int64_letoh(*((uint64_t *)(buffer + 1))) == expire_time_ms);
        }

        SECTION("buffer overflow") {
            uint64_t expire_time_ms = 123456;
            uint8_t buffer[7];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_opcode_expire_time_ms(
                        expire_time_ms,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW);
        }
    }

    SECTION("module_redis_snapshot_serialize_primitive_encode_opcode_value_type") {
        SECTION("normal case") {
            uint8_t buffer[1];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;

            module_snapshot_value_type_t value_type = MODULE_REDIS_SNAPSHOT_VALUE_TYPE_STRING;
            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_opcode_value_type(
                            value_type,
                            buffer,
                            buffer_size,
                            buffer_offset,
                            &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 1);
            REQUIRE(buffer[0] == value_type);
        }

        SECTION("buffer overflow") {
            uint8_t buffer[0];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;

            module_snapshot_value_type_t value_type = MODULE_REDIS_SNAPSHOT_VALUE_TYPE_STRING;
            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_opcode_value_type(
                            value_type,
                            buffer,
                            buffer_size,
                            buffer_offset,
                            &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW);
        }
    }

    SECTION("module_redis_snapshot_serialize_primitive_encode_key") {
        SECTION("normal case") {
            uint8_t buffer[10];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;
            char key[] = "example";
            size_t key_length = strlen(key);

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_key(
                        key,
                        key_length,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == key_length + 1); // 1 byte for length
            REQUIRE(buffer[0] == key_length); // Check length encoding
            REQUIRE(memcmp(buffer + 1, key, key_length) == 0); // Check key data
        }

        SECTION("buffer overflow") {
            uint8_t buffer[5];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;
            char key[] = "example";
            size_t key_length = strlen(key);

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_key(
                        key,
                        key_length,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW);
        }
    }

    SECTION("module_redis_snapshot_serialize_primitive_encode_string_length") {
        SECTION("normal case") {
            uint8_t buffer[10];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;
            size_t string_length = 7;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_string_length(
                        string_length,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 1); // 1 byte for length
            REQUIRE(buffer[0] == string_length); // Check length encoding
        }

        SECTION("buffer overflow") {
            uint8_t buffer[1];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;
            size_t string_length = 100;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_string_length(
                        string_length,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW);
        }
    }

    SECTION("module_redis_snapshot_serialize_primitive_encode_string_data_plain") {
        SECTION("normal case") {
            uint8_t buffer[10];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;
            char string[] = "example";
            size_t string_length = strlen(string);

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_string_data_plain(
                        string,
                        string_length,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == string_length);
            REQUIRE(memcmp(buffer, string, string_length) == 0); // Check string data
        }

        SECTION("buffer overflow") {
            uint8_t buffer[5];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;
            char string[] = "example";
            size_t string_length = strlen(string);

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_string_data_plain(
                        string,
                        string_length,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW);
        }

        SECTION("empty string") {
            uint8_t buffer[1];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;
            char string[] = "";
            size_t string_length = strlen(string);

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_string_data_plain(
                        string,
                        string_length,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 0);
        }
    }

    SECTION("module_redis_snapshot_serialize_primitive_encode_small_string_int") {
        SECTION("normal case - int8") {
            uint8_t buffer[10];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;
            int64_t string_integer = 42;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_small_string_int(
                        string_integer,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 2);
            REQUIRE(buffer[0] == 0xC0);
            REQUIRE(buffer[1] == (uint8_t) string_integer);
        }

        SECTION("normal case - int16") {
            uint8_t buffer[10];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;
            int64_t string_integer = 12345;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_small_string_int(
                        string_integer,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 3);
            REQUIRE(buffer[0] == (0xC0 | 0x01));
            REQUIRE(buffer[1] == (uint8_t) (string_integer & 0xFF));
            REQUIRE(buffer[2] == (uint8_t) ((string_integer >> 8) & 0xFF));
        }

        SECTION("normal case - int32") {
            uint8_t buffer[10];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;
            int64_t string_integer = 123456789;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_small_string_int(
                        string_integer,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 5);
            REQUIRE(buffer[0] == (0xC0 | 0x02));
            REQUIRE(buffer[1] == (uint8_t) (string_integer & 0xFF));
            REQUIRE(buffer[2] == (uint8_t) ((string_integer >> 8) & 0xFF));
            REQUIRE(buffer[3] == (uint8_t) ((string_integer >> 16) & 0xFF));
            REQUIRE(buffer[4] == (uint8_t) ((string_integer >> 24) & 0xFF));
        }

        SECTION("buffer overflow") {
            uint8_t buffer[2];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;
            size_t buffer_offset_out;
            int64_t string_integer = 123456789;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_small_string_int(
                        string_integer,
                        buffer,
                        buffer_size,
                        buffer_offset,
                        &buffer_offset_out);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW);
        }
    }

    SECTION("module_redis_snapshot_serialize_primitive_encode_small_string_lzf") {
        SECTION("normal case") {
            uint8_t buffer[256];
            size_t buffer_offset;
            char *string = "this is a very long string that should be compressed with a good compression ratio by the"
                           "lzf algorithm";
            size_t string_length = strlen(string);

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_small_string_lzf(
                            string,
                            string_length,
                            buffer,
                            sizeof(buffer),
                            0,
                            &buffer_offset);

            // Doesn't check the compressed length as it might vary between architectures and compilation flags
            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset > 4);
            REQUIRE(buffer[0] == (0xC0 | 0x03));

            // Extract from the buffer the string length and check if it matches the initial one
            uint32_t string_length_from_buffer = 0;
            string_length_from_buffer |= (buffer[3] & (uint8_t)(~0xC0)) << 8;
            string_length_from_buffer |= buffer[4];
            REQUIRE(string_length_from_buffer == string_length);

            // Read the compressed length from buffer[1] to buffer[4] inclusive and checks that the compressed length is
            // smaller than the original string
            uint32_t compressed_length = 0;
            compressed_length |= (buffer[1] & (uint8_t)(~0xC0)) << 8;
            compressed_length |= buffer[2];
            REQUIRE(compressed_length < string_length);

            // Try to decompress the data
            char decompressed[1024];
            size_t decompressed_length = lzf_decompress(
                    buffer + 5,
                    buffer_offset - 5,
                    decompressed,
                    sizeof(decompressed));

            // Check if the decompressed data match
            REQUIRE(decompressed_length == string_length);
            REQUIRE(memcmp(decompressed, string, string_length) == 0);
        }

        SECTION("buffer overflow") {
            uint8_t buffer[256];
            size_t buffer_offset;
            char *string = "hello world";
            size_t string_length = strlen(string);

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_small_string_lzf(
                            string, string_length, buffer, 1, 0, &buffer_offset);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW);
        }

        SECTION("compression ratio too low") {
            uint8_t buffer[256];
            size_t buffer_offset;
            char *string = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
                           "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
                           "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
                           "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f";
            size_t string_length = 64;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_small_string_lzf(
                            string,
                            string_length,
                            buffer,
                            sizeof(buffer),
                            0,
                            &buffer_offset);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_COMPRESSION_RATIO_TOO_LOW);
        }
    }

    SECTION("module_redis_snapshot_serialize_primitive_encode_opcode_eof") {
        SECTION("normal case") {
            uint8_t buffer[256];
            size_t buffer_offset;
            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_opcode_eof(
                            0, buffer, sizeof(buffer), 0, &buffer_offset);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset == 9);
            REQUIRE(buffer[0] == MODULE_REDIS_SNAPSHOT_OPCODE_EOF);
            REQUIRE(buffer[1] == 0);
            REQUIRE(buffer[2] == 0);
            REQUIRE(buffer[3] == 0);
            REQUIRE(buffer[4] == 0);
            REQUIRE(buffer[5] == 0);
            REQUIRE(buffer[6] == 0);
            REQUIRE(buffer[7] == 0);
            REQUIRE(buffer[8] == 0);
        }

        SECTION("buffer overflow") {
            uint8_t buffer[256];
            size_t buffer_offset;
            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_opcode_eof(
                            0, buffer, 0, 0, &buffer_offset);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW);
        }
    }

    SECTION("module_redis_snapshot_serialize_primitive_encode_small_string") {
        SECTION("encoding small string as integer") {
            uint8_t buffer[100];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;

            char string[] = "1234";
            size_t string_length = strlen(string);

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_small_string(
                            string,
                            string_length,
                            buffer,
                            sizeof(buffer),
                            0,
                            &buffer_offset);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset == 3);
            REQUIRE(buffer[0] == (0xC0 | 0x01));
            REQUIRE(buffer[1] == 0xD2);
            REQUIRE(buffer[2] == 0x04);
        }

        SECTION("encoding small string as plain data (<32 bytes)") {
            uint8_t buffer[100];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;

            char string[] = "This is a small string";
            size_t string_length = strlen(string);

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_small_string(
                            string,
                            string_length,
                            buffer,
                            sizeof(buffer),
                            0,
                            &buffer_offset);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset == 1 + string_length);
            REQUIRE(buffer[0] == string_length);
            REQUIRE(memcmp(buffer + 1, string, string_length) == 0);
        }

        SECTION("encoding small string as plain data (compression ratio too low)") {
            uint8_t buffer[100];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset = 0;

            char *string = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
                           "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
                           "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f";
            size_t string_length = 48;

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_small_string(
                            string,
                            string_length,
                            buffer,
                            sizeof(buffer),
                            0,
                            &buffer_offset);

            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset == 1 + string_length);
            REQUIRE(buffer[0] == string_length);
            REQUIRE(memcmp(buffer + 1, string, string_length) == 0);
        }

        SECTION("encoding small string with LZF compression") {
            uint8_t buffer[256];
            size_t buffer_offset;
            char *string = "this is a very long string that should be compressed with a good compression ratio by the"
                           "lzf algorithm";
            size_t string_length = strlen(string);

            module_redis_snapshot_serialize_primitive_result_t result =
                    module_redis_snapshot_serialize_primitive_encode_small_string(
                            string,
                            string_length,
                            buffer,
                            sizeof(buffer),
                            0,
                            &buffer_offset);

            // Doesn't check the compressed length as it might vary between architectures and compilation flags
            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset > 4);
            REQUIRE(buffer[0] == (0xC0 | 0x03));

            // Extract from the buffer the string length and check if it matches the initial one
            uint32_t string_length_from_buffer = 0;
            string_length_from_buffer |= (buffer[3] & (uint8_t)(~0xC0)) << 8;
            string_length_from_buffer |= buffer[4];
            REQUIRE(string_length_from_buffer == string_length);

            // Read the compressed length from buffer[1] to buffer[4] inclusive and checks that the compressed length is
            // smaller than the original string
            uint32_t compressed_length = 0;
            compressed_length |= (buffer[1] & (uint8_t)(~0xC0)) << 8;
            compressed_length |= buffer[2];
            REQUIRE(compressed_length < string_length);

            // Try to decompress the data
            char decompressed[1024];
            size_t decompressed_length = lzf_decompress(
                    buffer + 5,
                    buffer_offset - 5,
                    decompressed,
                    sizeof(decompressed));

            // Check if the decompressed data match
            REQUIRE(decompressed_length == string_length);
            REQUIRE(memcmp(decompressed, string, string_length) == 0);
        }
    }

    SECTION("module_redis_snapshot_serialize_primitive_encode_opcode_aux") {
        uint8_t buffer[1024];
        size_t buffer_offset = 0;
        size_t buffer_offset_out = 0;
        char key[] = "test_key";
        char value[] = "test_value";
        size_t key_length = strlen(key);
        size_t value_length = strlen(value);
        module_redis_snapshot_serialize_primitive_result_t result;

        SECTION("normal case") {
            result = module_redis_snapshot_serialize_primitive_encode_opcode_aux(
                    key,
                    key_length,
                    value,
                    value_length,
                    buffer,
                    sizeof(buffer),
                    buffer_offset,
                    &buffer_offset_out);
            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            REQUIRE(buffer_offset_out == 1 + 1 + key_length + 1 + value_length);
            REQUIRE(buffer[0] == MODULE_REDIS_SNAPSHOT_OPCODE_AUX);
            REQUIRE(memcmp(buffer + 1 + 1, key, key_length) == 0);
            REQUIRE(memcmp(buffer + 1 + + 1 + key_length + 1, value, value_length) == 0);
        }

        SECTION("buffer_overflow") {
            buffer_offset = 1020;
            result = module_redis_snapshot_serialize_primitive_encode_opcode_aux(
                    key,
                    key_length,
                    value,
                    value_length,
                    buffer,
                    sizeof(buffer),
                    buffer_offset,
                    &buffer_offset_out);
            REQUIRE(result == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_BUFFER_OVERFLOW);
        }
    }

    SECTION("file format compatibility") {
        if (test_module_redis_snapshot_serialize_primitive_redis_check_rdb_available() == false) {
            WARN("Can't test RDB compatibility as redis-check-rdb is not available");
            return;
        }
        uint8_t rdb_version = 9;

        SECTION("generate an empty rdb") {
            uint8_t buffer[1024];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset_out = 0;
            module_redis_snapshot_header_t module_redis_snapshot_header = { .version = rdb_version };

            REQUIRE(module_redis_snapshot_serialize_primitive_encode_header(
                    &module_redis_snapshot_header,
                    buffer,
                    buffer_size,
                    buffer_offset_out,
                    &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

            REQUIRE(module_redis_snapshot_serialize_primitive_encode_opcode_db_number(
                    0,
                    buffer,
                    buffer_size,
                    buffer_offset_out,
                    &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

            REQUIRE(module_redis_snapshot_serialize_primitive_encode_opcode_eof(
                    0,
                    buffer,
                    buffer_size,
                    buffer_offset_out,
                    &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

            // Write the data
            REQUIRE(test_module_redis_snapshot_serialize_primitive_vaidate_rdb((char*)buffer, buffer_offset_out));
        }

        SECTION("generate an empty rdb with aux") {
            const char aux_key[] = "cachegrand-version";
            const char *aux_value = CACHEGRAND_CMAKE_CONFIG_VERSION_GIT;
            uint8_t buffer[1024];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset_out = 0;
            module_redis_snapshot_header_t module_redis_snapshot_header = { .version = rdb_version };

            REQUIRE(module_redis_snapshot_serialize_primitive_encode_header(
                    &module_redis_snapshot_header,
                    buffer,
                    buffer_size,
                    buffer_offset_out,
                    &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

            REQUIRE(module_redis_snapshot_serialize_primitive_encode_opcode_aux(
                    (char*)aux_key,
                    sizeof(aux_key),
                    (char*)aux_value,
                    sizeof(aux_value),
                    buffer,
                    buffer_size,
                    buffer_offset_out,
                    &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

            REQUIRE(module_redis_snapshot_serialize_primitive_encode_opcode_db_number(
                    0,
                    buffer,
                    buffer_size,
                    buffer_offset_out,
                    &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

            REQUIRE(module_redis_snapshot_serialize_primitive_encode_opcode_eof(
                    0,
                    buffer,
                    buffer_size,
                    buffer_offset_out,
                    &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

            // Write the data
            REQUIRE(test_module_redis_snapshot_serialize_primitive_vaidate_rdb((char*)buffer, buffer_offset_out));
        }

        SECTION("generate an rdb with 5 random strings without expiration, without string integers and compression") {
            uint8_t buffer[1024];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset_out = 0;
            module_redis_snapshot_header_t module_redis_snapshot_header = { .version = rdb_version };

            REQUIRE(module_redis_snapshot_serialize_primitive_encode_header(
                    &module_redis_snapshot_header,
                    buffer,
                    buffer_size,
                    buffer_offset_out,
                    &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

            REQUIRE(module_redis_snapshot_serialize_primitive_encode_opcode_db_number(
                    0,
                    buffer,
                    buffer_size,
                    buffer_offset_out,
                    &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

            for (int i = 0; i < 5; i++) {
                char key[10];
                char string[10];
                sprintf(key, "key%d", i);
                size_t key_length = strlen(key);
                sprintf(string, "value%d", i);
                size_t string_length = strlen(string);

                REQUIRE(module_redis_snapshot_serialize_primitive_encode_opcode_value_type(
                        MODULE_REDIS_SNAPSHOT_VALUE_TYPE_STRING,
                        buffer,
                        buffer_size,
                        buffer_offset_out,
                        &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

                REQUIRE(module_redis_snapshot_serialize_primitive_encode_key(
                        key,
                        key_length,
                        buffer,
                        buffer_size,
                        buffer_offset_out,
                        &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

                REQUIRE(module_redis_snapshot_serialize_primitive_encode_string_length(
                        string_length,
                        buffer,
                        buffer_size,
                        buffer_offset_out,
                        &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

                REQUIRE(module_redis_snapshot_serialize_primitive_encode_string_data_plain(
                        string,
                        string_length,
                        buffer,
                        buffer_size,
                        buffer_offset_out,
                        &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            }

            REQUIRE(module_redis_snapshot_serialize_primitive_encode_opcode_eof(
                    0,
                    buffer,
                    buffer_size,
                    buffer_offset_out,
                    &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

            // Write the data
            REQUIRE(test_module_redis_snapshot_serialize_primitive_vaidate_rdb((char*)buffer, buffer_offset_out));
        }

        SECTION("generate an rdb with 3 random strings using integers (<=INT8_MAX, <=INT16_MAX and <= INT32_MAX)") {
            uint8_t buffer[1024];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset_out = 0;
            module_redis_snapshot_header_t module_redis_snapshot_header = { .version = rdb_version };

            REQUIRE(module_redis_snapshot_serialize_primitive_encode_header(
                    &module_redis_snapshot_header,
                    buffer,
                    buffer_size,
                    buffer_offset_out,
                    &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

            REQUIRE(module_redis_snapshot_serialize_primitive_encode_opcode_db_number(
                    0,
                    buffer,
                    buffer_size,
                    buffer_offset_out,
                    &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

            for (int i = 0; i < 3; i++) {
                int64_t string_integer_out;
                char key[10];
                char string[20];
                sprintf(key, "key%d", i);
                size_t key_length = strlen(key);
                if (i == 0) {
                    sprintf(string, "%d", INT8_MAX);
                } else if (i == 1) {
                    sprintf(string, "%d", INT16_MAX);
                } else if (i == 2) {
                    sprintf(string, "%d", INT32_MAX);
                }
                size_t string_length = strlen(string);

                REQUIRE(module_redis_snapshot_serialize_primitive_encode_opcode_value_type(
                        MODULE_REDIS_SNAPSHOT_VALUE_TYPE_STRING,
                        buffer,
                        buffer_size,
                        buffer_offset_out,
                        &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

                REQUIRE(module_redis_snapshot_serialize_primitive_encode_key(
                        key,
                        key_length,
                        buffer,
                        buffer_size,
                        buffer_offset_out,
                        &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

                REQUIRE(module_redis_snapshot_serialize_primitive_can_encode_string_int(
                        string,
                        string_length,
                        &string_integer_out));

                REQUIRE(module_redis_snapshot_serialize_primitive_encode_small_string_int(
                        string_integer_out,
                        buffer,
                        buffer_size,
                        buffer_offset_out,
                        &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            }

            REQUIRE(module_redis_snapshot_serialize_primitive_encode_opcode_eof(
                    0,
                    buffer,
                    buffer_size,
                    buffer_offset_out,
                    &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

            // Write the data
            REQUIRE(test_module_redis_snapshot_serialize_primitive_vaidate_rdb((char*)buffer, buffer_offset_out));
        }

        SECTION("generate an rdb with strings with a random length between 32 and 65535") {
            // Needs plenty of space as the strings to compress are large
            uint8_t buffer[65535 * 6];
            size_t buffer_size = sizeof(buffer);
            size_t buffer_offset_out = 0;
            module_redis_snapshot_header_t module_redis_snapshot_header = { .version = rdb_version };

            REQUIRE(module_redis_snapshot_serialize_primitive_encode_header(
                    &module_redis_snapshot_header,
                    buffer,
                    buffer_size,
                    buffer_offset_out,
                    &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

            REQUIRE(module_redis_snapshot_serialize_primitive_encode_opcode_db_number(
                    0,
                    buffer,
                    buffer_size,
                    buffer_offset_out,
                    &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

            for (int i = 0; i < 5; i++) {
                char key[10];
                char string[65535];
                sprintf(key, "key%d", i);
                size_t key_length = strlen(key);
                size_t string_length = random_generate() % (65535 - 32) + 32;
                for (int j = 0; j < string_length; j++) {
                    string[j] = 'a';
                }

                REQUIRE(module_redis_snapshot_serialize_primitive_encode_opcode_value_type(
                        MODULE_REDIS_SNAPSHOT_VALUE_TYPE_STRING,
                        buffer,
                        buffer_size,
                        buffer_offset_out,
                        &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

                REQUIRE(module_redis_snapshot_serialize_primitive_encode_key(
                        key,
                        key_length,
                        buffer,
                        buffer_size,
                        buffer_offset_out,
                        &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

                REQUIRE(module_redis_snapshot_serialize_primitive_encode_small_string_lzf(
                        string,
                        string_length,
                        buffer,
                        buffer_size,
                        buffer_offset_out,
                        &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);
            }

            REQUIRE(module_redis_snapshot_serialize_primitive_encode_opcode_eof(
                    0,
                    buffer,
                    buffer_size,
                    buffer_offset_out,
                    &buffer_offset_out) == MODULE_REDIS_SNAPSHOT_SERIALIZE_PRIMITIVE_RESULT_OK);

            // Write the data
            REQUIRE(test_module_redis_snapshot_serialize_primitive_vaidate_rdb((char*)buffer, buffer_offset_out));
        }
    }
}
