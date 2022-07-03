/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <string.h>
#include <catch2/catch.hpp>

#include <unistd.h>
#include <sys/syscall.h>

#include "support/simple_file_io.h"

bool test_simple_file_io_fixture_file_from_data_create(
        char* path,
        int path_suffix_len,
        const char* data,
        size_t data_len) {

    close(mkstemps(path, path_suffix_len));

    FILE* fp = fopen(path, "w");
    if (fp == NULL) {
        return false;
    }

    size_t res;
    if ((res = fwrite(data, 1, data_len, fp)) != data_len) {
        fclose(fp);
        unlink(path);
        return false;
    }

    if (fflush(fp) != 0) {
        fclose(fp);
        unlink(path);
        return false;
    }

    fclose(fp);

    return true;
}

void test_simple_file_io_fixture_file_from_data_cleanup(
        const char* path) {
    unlink(path);
}

#define TEST_SIMPLE_FILE_IO_FIXTURE_FILE_FROM_DATA(DATA, DATA_LEN, CONFIG_PATH, ...) { \
    { \
        char CONFIG_PATH[] = "/tmp/cachegrand-tests-XXXXXX.tmp"; \
        int CONFIG_PATH_suffix_len = 4; /** .tmp **/ \
        REQUIRE(test_simple_file_io_fixture_file_from_data_create(CONFIG_PATH, CONFIG_PATH_suffix_len, DATA, DATA_LEN)); \
        __VA_ARGS__; \
        test_simple_file_io_fixture_file_from_data_cleanup(CONFIG_PATH); \
    } \
}

TEST_CASE("support/simple_file_io.c", "[support][simple_file_io]") {
    const char* simple_file_io_fixture_data_invalid_no_newline_uint32_str = "12345";
    const char* simple_file_io_fixture_data_invalid_only_newline_uint32_str = "\n";
    const char* simple_file_io_fixture_data_invalid_empty_uint32_str = "";
    const char* simple_file_io_fixture_data_valid_uint32_str = "12345\n";
    uint32_t simple_file_io_fixture_data_valid_uint32 = 12345;

    SECTION("simple_file_io_read") {
        SECTION("valid number") {
            char buf[512];
            TEST_SIMPLE_FILE_IO_FIXTURE_FILE_FROM_DATA(
                    simple_file_io_fixture_data_valid_uint32_str,
                    strlen(simple_file_io_fixture_data_valid_uint32_str),
                    uint32_file_path,
                    {
                        REQUIRE(simple_file_io_read(uint32_file_path, buf, sizeof(buf)));
                    });

            REQUIRE(strcmp(buf, simple_file_io_fixture_data_valid_uint32_str) == 0);
        }

        SECTION("invalid number - no newline") {
            char buf[512];
            TEST_SIMPLE_FILE_IO_FIXTURE_FILE_FROM_DATA(
                    simple_file_io_fixture_data_invalid_no_newline_uint32_str,
                    strlen(simple_file_io_fixture_data_invalid_no_newline_uint32_str),
                    uint32_file_path,
                    {
                        REQUIRE(simple_file_io_read(uint32_file_path, buf, sizeof(buf)));
                    });

            REQUIRE(strcmp(buf, simple_file_io_fixture_data_invalid_no_newline_uint32_str) == 0);
        }

        SECTION("invalid path") {
            char buf[512];
            REQUIRE(!simple_file_io_read("/non/existing/path/leading/nowhere", buf, sizeof(buf)));
        }
    }

    SECTION("simple_file_io_read_uint32") {
        SECTION("valid number") {
            uint32_t val;
            TEST_SIMPLE_FILE_IO_FIXTURE_FILE_FROM_DATA(
                    simple_file_io_fixture_data_valid_uint32_str,
                    strlen(simple_file_io_fixture_data_valid_uint32_str),
                    uint32_file_path,
                    {
                        REQUIRE(simple_file_io_read_uint32(uint32_file_path, &val));
                    });

            REQUIRE(val == simple_file_io_fixture_data_valid_uint32);
        }

        SECTION("invalid number - no newline") {
            uint32_t val;
            TEST_SIMPLE_FILE_IO_FIXTURE_FILE_FROM_DATA(
                    simple_file_io_fixture_data_invalid_no_newline_uint32_str,
                    strlen(simple_file_io_fixture_data_invalid_no_newline_uint32_str),
                    uint32_file_path,
                    {
                        REQUIRE(!simple_file_io_read_uint32(uint32_file_path, &val));
                    });

            REQUIRE(val == 12345);
        }

        SECTION("invalid number - only newline") {
            uint32_t val;
            TEST_SIMPLE_FILE_IO_FIXTURE_FILE_FROM_DATA(
                    simple_file_io_fixture_data_invalid_only_newline_uint32_str,
                    strlen(simple_file_io_fixture_data_invalid_only_newline_uint32_str),
                    uint32_file_path,
                    {
                        REQUIRE(!simple_file_io_read_uint32(uint32_file_path, &val));
                    });

            REQUIRE(val == 0);
        }

        SECTION("invalid number - empty") {
            uint32_t val;
            TEST_SIMPLE_FILE_IO_FIXTURE_FILE_FROM_DATA(
                    simple_file_io_fixture_data_invalid_empty_uint32_str,
                    strlen(simple_file_io_fixture_data_invalid_empty_uint32_str),
                    uint32_file_path,
                    {
                        REQUIRE(!simple_file_io_read_uint32(uint32_file_path, &val));
                    });

            REQUIRE(val == 0);
        }

        SECTION("invalid path") {
            uint32_t val;
            REQUIRE(!simple_file_io_read_uint32("/non/existing/path/leading/nowhere", &val));
        }
    }

    SECTION("simple_file_io_read_uint32_return") {
        SECTION("valid number") {
            uint32_t val;
            TEST_SIMPLE_FILE_IO_FIXTURE_FILE_FROM_DATA(
                    simple_file_io_fixture_data_valid_uint32_str,
                    strlen(simple_file_io_fixture_data_valid_uint32_str),
                    uint32_file_path,
                    {
                        val = simple_file_io_read_uint32_return(uint32_file_path);
                    });

            REQUIRE(val == simple_file_io_fixture_data_valid_uint32);
        }

        SECTION("invalid number - no newline") {
            uint32_t val;
            TEST_SIMPLE_FILE_IO_FIXTURE_FILE_FROM_DATA(
                    simple_file_io_fixture_data_invalid_no_newline_uint32_str,
                    strlen(simple_file_io_fixture_data_invalid_no_newline_uint32_str),
                    uint32_file_path,
                    {
                        val = simple_file_io_read_uint32_return(uint32_file_path);
                    });

            REQUIRE(val == 0);
        }

        SECTION("invalid number - only newline") {
            uint32_t val;
            TEST_SIMPLE_FILE_IO_FIXTURE_FILE_FROM_DATA(
                    simple_file_io_fixture_data_invalid_only_newline_uint32_str,
                    strlen(simple_file_io_fixture_data_invalid_only_newline_uint32_str),
                    uint32_file_path,
                    {
                        val = simple_file_io_read_uint32_return(uint32_file_path);
                    });

            REQUIRE(val == 0);
        }

        SECTION("invalid number - empty") {
            uint32_t val;
            TEST_SIMPLE_FILE_IO_FIXTURE_FILE_FROM_DATA(
                    simple_file_io_fixture_data_invalid_empty_uint32_str,
                    strlen(simple_file_io_fixture_data_invalid_empty_uint32_str),
                    uint32_file_path,
                    {
                        val = simple_file_io_read_uint32_return(uint32_file_path);
                    });

            REQUIRE(val == 0);
        }

        SECTION("invalid path") {
            REQUIRE(simple_file_io_read_uint32_return("/non/existing/path/leading/nowhere") == 0);
        }
    }
}
