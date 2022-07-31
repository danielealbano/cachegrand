/**
 * Copyright (C) 2018-2022 Vito Castellano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include "support/sentry/sentry_support.h"

TEST_CASE("support/sentry/sentry_support.c", "[support][sentry][sentry_support]") {
    SECTION("sentry_support_init") {
        SECTION("null params") {
            sentry_support_init(nullptr, nullptr);
        }

        SECTION("with params") {
            char data_path[] = "/fake/path/no/fail";
            char dsn[] = "/fake/dsn";

            sentry_support_init(data_path, dsn);
        }
    }
}