#include <catch2/catch.hpp>

#include "support/simple_file_io.h"
#include "hugepages.h"

TEST_CASE("hugepages.c", "[hugepages]") {
    SECTION("hugepages_2mb_is_available") {
        const char* free_hugepages_2mb_path = HUGEPAGES_SYSFS_2MB_PATH HUGEPAGES_SYSFS_FREE_HUGEPAGES_FILENAME;
        const char* resv_hugepages_2mb_path = HUGEPAGES_SYSFS_2MB_PATH HUGEPAGES_SYSFS_RESV_HUGEPAGES_FILENAME;

        uint32_t free_hugepages = simple_file_io_read_uint32_return(free_hugepages_2mb_path);
        uint32_t resv_hugepages = simple_file_io_read_uint32_return(resv_hugepages_2mb_path);

        bool has_at_least_one_hugepage_2mb = (free_hugepages - resv_hugepages) > 0;
        REQUIRE(hugepages_2mb_is_available(0) == has_at_least_one_hugepage_2mb);
    }

    SECTION("hugepages_1024mb_is_available") {
        const char* free_hugepages_1024mb_path = HUGEPAGES_SYSFS_1024MB_PATH HUGEPAGES_SYSFS_FREE_HUGEPAGES_FILENAME;
        const char* resv_hugepages_1024mb_path = HUGEPAGES_SYSFS_1024MB_PATH HUGEPAGES_SYSFS_RESV_HUGEPAGES_FILENAME;

        uint32_t free_hugepages = simple_file_io_read_uint32_return(free_hugepages_1024mb_path);
        uint32_t resv_hugepages = simple_file_io_read_uint32_return(resv_hugepages_1024mb_path);

        bool has_at_least_one_hugepage_1024mb = (free_hugepages - resv_hugepages) > 0;
        REQUIRE(hugepages_1024mb_is_available(0) == has_at_least_one_hugepage_1024mb);
    }
}
