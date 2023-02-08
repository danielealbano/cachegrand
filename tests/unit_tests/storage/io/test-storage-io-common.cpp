/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>
#include <unistd.h>

#include "storage/io/storage_io_common.h"

TEST_CASE("storage/io/storage_io_common.c", "[storage][storage_io][storage_io_common]") {
    SECTION("storage_io_common_close") {
        char fixture_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
        int fixture_temp_path_suffix_len = 4;

        SECTION("invalid fd") {
            int fd = -1;
            REQUIRE(storage_io_common_close(fd) == false);
        }

        SECTION("valid fd") {
            int fd = mkstemps(fixture_temp_path, fixture_temp_path_suffix_len);
            REQUIRE(storage_io_common_close(fd));
            unlink(fixture_temp_path);
        }
    }
}
