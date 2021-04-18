#include <string.h>
#include <catch2/catch.hpp>

#include <unistd.h>
#include <sys/syscall.h>

#include "hugepages.h"

bool test_hugepages_fixture_file_from_data_create(
        char* path,
        int path_suffix_len,
        const char* data,
        size_t data_len) {

    if (!mkstemps(path, path_suffix_len)) {
        return false;
    }

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

void test_hugepages_fixture_file_from_data_cleanup(
        const char* path) {
    unlink(path);
}

#define TEST_HUGEPAGES_FIXTURE_FILE_FROM_DATA(DATA, DATA_LEN, CONFIG_PATH, ...) { \
    { \
        char CONFIG_PATH[] = "/tmp/cachegrand-tests-XXXXXX.tmp"; \
        int CONFIG_PATH_suffix_len = 4; /** .tmp **/ \
        REQUIRE(test_hugepages_fixture_file_from_data_create(CONFIG_PATH, CONFIG_PATH_suffix_len, DATA, DATA_LEN)); \
        __VA_ARGS__; \
        test_hugepages_fixture_file_from_data_cleanup(CONFIG_PATH); \
    } \
}

TEST_CASE("hugepages.c", "[hugepages]") {
    const char* hugepages_fixture_data_invalid_no_newline_uint32_str = "12345";
    const char* hugepages_fixture_data_invalid_only_newline_uint32_str = "\n";
    const char* hugepages_fixture_data_invalid_empty_uint32_str = "";
    const char* hugepages_fixture_data_valid_uint32_str = "12345\n";
    uint32_t hugepages_fixture_data_valid_uint32 = 12345;

    SECTION("hugepages_file_read") {
        SECTION("valid number") {
            char buf[512];
            TEST_HUGEPAGES_FIXTURE_FILE_FROM_DATA(
                    hugepages_fixture_data_valid_uint32_str,
                    strlen(hugepages_fixture_data_valid_uint32_str),
                    uint32_file_path,
                    {
                        REQUIRE(hugepages_file_read(uint32_file_path, buf, sizeof(buf)));
                    });

            REQUIRE(strcmp(buf, hugepages_fixture_data_valid_uint32_str) == 0);
        }

        SECTION("invalid number - no newline") {
            char buf[512];
            TEST_HUGEPAGES_FIXTURE_FILE_FROM_DATA(
                    hugepages_fixture_data_invalid_no_newline_uint32_str,
                    strlen(hugepages_fixture_data_invalid_no_newline_uint32_str),
                    uint32_file_path,
                    {
                        REQUIRE(hugepages_file_read(uint32_file_path, buf, sizeof(buf)));
                    });

            REQUIRE(strcmp(buf, hugepages_fixture_data_invalid_no_newline_uint32_str) == 0);
        }
    }

    SECTION("hugepages_file_read_uint32") {
        SECTION("valid number") {
            uint32_t val;
            TEST_HUGEPAGES_FIXTURE_FILE_FROM_DATA(
                    hugepages_fixture_data_valid_uint32_str,
                    strlen(hugepages_fixture_data_valid_uint32_str),
                    uint32_file_path,
                    {
                        REQUIRE(hugepages_file_read_uint32(uint32_file_path, &val));
                    });

            REQUIRE(val == hugepages_fixture_data_valid_uint32);
        }

        SECTION("invalid number - no newline") {
            uint32_t val;
            TEST_HUGEPAGES_FIXTURE_FILE_FROM_DATA(
                    hugepages_fixture_data_invalid_no_newline_uint32_str,
                    strlen(hugepages_fixture_data_invalid_no_newline_uint32_str),
                    uint32_file_path,
                    {
                        REQUIRE(!hugepages_file_read_uint32(uint32_file_path, &val));
                    });

            REQUIRE(val == 12345);
        }

        SECTION("invalid number - only newline") {
            uint32_t val;
            TEST_HUGEPAGES_FIXTURE_FILE_FROM_DATA(
                    hugepages_fixture_data_invalid_only_newline_uint32_str,
                    strlen(hugepages_fixture_data_invalid_only_newline_uint32_str),
                    uint32_file_path,
                    {
                        REQUIRE(!hugepages_file_read_uint32(uint32_file_path, &val));
                    });

            REQUIRE(val == 0);
        }

        SECTION("invalid number - empty") {
            uint32_t val;
            TEST_HUGEPAGES_FIXTURE_FILE_FROM_DATA(
                    hugepages_fixture_data_invalid_empty_uint32_str,
                    strlen(hugepages_fixture_data_invalid_empty_uint32_str),
                    uint32_file_path,
                    {
                        REQUIRE(!hugepages_file_read_uint32(uint32_file_path, &val));
                    });

            REQUIRE(val == 0);
        }
    }

    SECTION("hugepages_file_path_uint32_return") {
        SECTION("valid number") {
            uint32_t val;
            TEST_HUGEPAGES_FIXTURE_FILE_FROM_DATA(
                    hugepages_fixture_data_valid_uint32_str,
                    strlen(hugepages_fixture_data_valid_uint32_str),
                    uint32_file_path,
                    {
                        val = hugepages_file_path_uint32_return(uint32_file_path);
                    });

            REQUIRE(val == hugepages_fixture_data_valid_uint32);
        }

        SECTION("invalid number - no newline") {
            uint32_t val;
            TEST_HUGEPAGES_FIXTURE_FILE_FROM_DATA(
                    hugepages_fixture_data_invalid_no_newline_uint32_str,
                    strlen(hugepages_fixture_data_invalid_no_newline_uint32_str),
                    uint32_file_path,
                    {
                        val = hugepages_file_path_uint32_return(uint32_file_path);
                    });

            REQUIRE(val == 0);
        }

        SECTION("invalid number - only newline") {
            uint32_t val;
            TEST_HUGEPAGES_FIXTURE_FILE_FROM_DATA(
                    hugepages_fixture_data_invalid_only_newline_uint32_str,
                    strlen(hugepages_fixture_data_invalid_only_newline_uint32_str),
                    uint32_file_path,
                    {
                        val = hugepages_file_path_uint32_return(uint32_file_path);
                    });

            REQUIRE(val == 0);
        }

        SECTION("invalid number - empty") {
            uint32_t val;
            TEST_HUGEPAGES_FIXTURE_FILE_FROM_DATA(
                    hugepages_fixture_data_invalid_empty_uint32_str,
                    strlen(hugepages_fixture_data_invalid_empty_uint32_str),
                    uint32_file_path,
                    {
                        val = hugepages_file_path_uint32_return(uint32_file_path);
                    });

            REQUIRE(val == 0);
        }
    }

    SECTION("hugepages_2mb_is_available") {
        const char* free_hugepages_2mb_path = HUGEPAGES_SYSFS_2MB_PATH HUGEPAGES_SYSFS_FREE_HUGEPAGES_FILENAME;
        const char* resv_hugepages_2mb_path = HUGEPAGES_SYSFS_2MB_PATH HUGEPAGES_SYSFS_RESV_HUGEPAGES_FILENAME;

        uint32_t free_hugepages = hugepages_file_path_uint32_return(free_hugepages_2mb_path);
        uint32_t resv_hugepages = hugepages_file_path_uint32_return(resv_hugepages_2mb_path);

        bool has_at_least_one_hugepage_2mb = (free_hugepages - resv_hugepages) > 0;
        REQUIRE(hugepages_2mb_is_available(0) == has_at_least_one_hugepage_2mb);
    }

    SECTION("hugepages_1024mb_is_available") {
        const char* free_hugepages_1024mb_path = HUGEPAGES_SYSFS_1024MB_PATH HUGEPAGES_SYSFS_FREE_HUGEPAGES_FILENAME;
        const char* resv_hugepages_1024mb_path = HUGEPAGES_SYSFS_1024MB_PATH HUGEPAGES_SYSFS_RESV_HUGEPAGES_FILENAME;

        uint32_t free_hugepages = hugepages_file_path_uint32_return(free_hugepages_1024mb_path);
        uint32_t resv_hugepages = hugepages_file_path_uint32_return(resv_hugepages_1024mb_path);

        bool has_at_least_one_hugepage_1024mb = (free_hugepages - resv_hugepages) > 0;
        REQUIRE(hugepages_1024mb_is_available(0) == has_at_least_one_hugepage_1024mb);
    }
}
