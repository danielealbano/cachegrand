/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <string.h>
#include <catch2/catch_test_macros.hpp>

#include "misc.h"
#include "utils_string.h"

const char string_32_a[32] = "this is a string";
const char string_32_b[32] = "this is string a";
const char string_32_c[32] = "string with a different length";
const char string_32_d[32] = "this is a string but different";
const char string_32_e[32] = "THIS IS A STRING";
const char string_50_f[64] = "01234567890123456789012345678901234567890123456789";
const char string_50_g[64] = "ABCDEFGHIK0123456789012345678901234567890123456789";
const char string_60_h[64] = "012345678901234567890123456789012345678901234567890123456789";

#define TEST_UTILS_STRING_PLATFORM_DEPENDENT(SUFFIX) \
    SECTION("utils_string_cmp_eq_32" STRINGIZE(SUFFIX)) { \
        SECTION("same string lower case") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_32_a, strlen(string_32_a), string_32_a, strlen(string_32_a)) == true); \
        } \
        SECTION("same string upper case") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_32_e, strlen(string_32_e), string_32_e, strlen(string_32_e)) == true); \
        } \
        SECTION("different string with different length") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_32_a, strlen(string_32_a), string_32_c, strlen(string_32_c)) == false); \
        } \
        SECTION("different string with same length") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_32_b, strlen(string_32_b), string_32_c, strlen(string_32_c)) == false); \
        } \
        SECTION("different string same start and same length") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_32_a, strlen(string_32_a), string_32_b, strlen(string_32_b)) == false); \
        } \
        SECTION("different string same letters different case") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_32_a, strlen(string_32_a), string_32_e, strlen(string_32_e)) == false); \
        } \
        SECTION("same string same letters same case longer than 32") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_50_f, strlen(string_50_f), string_50_f, strlen(string_50_f)) == true); \
        } \
        SECTION("different string same letters same case longer than 32") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_50_f, strlen(string_50_f), string_50_g, strlen(string_50_g)) == false); \
        } \
        SECTION("different string same letters same case longer different length than 32") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_50_f, strlen(string_50_f), string_60_h, strlen(string_60_h)) == false); \
        } \
    } \
    SECTION("utils_string_casecmp_eq_32" STRINGIZE(SUFFIX)) { \
        SECTION("same string lower case") { \
            REQUIRE(utils_string_casecmp_eq_32##SUFFIX(string_32_a, strlen(string_32_a), string_32_a, strlen(string_32_a)) == true); \
        } \
        SECTION("same string upper case") { \
            REQUIRE(utils_string_casecmp_eq_32##SUFFIX(string_32_e, strlen(string_32_e), string_32_e, strlen(string_32_e)) == true); \
        } \
        SECTION("different string with different length") { \
            REQUIRE(utils_string_casecmp_eq_32##SUFFIX(string_32_a, strlen(string_32_a), string_32_c, strlen(string_32_c)) == false); \
        } \
        SECTION("different string with same length") { \
            REQUIRE(utils_string_casecmp_eq_32##SUFFIX(string_32_b, strlen(string_32_b), string_32_c, strlen(string_32_c)) == false); \
        } \
        SECTION("different string same start and same length") { \
            REQUIRE(utils_string_casecmp_eq_32##SUFFIX(string_32_a, strlen(string_32_a), string_32_b, strlen(string_32_b)) == false); \
        } \
        SECTION("different string same letters different case") { \
            REQUIRE(utils_string_casecmp_eq_32##SUFFIX(string_32_a, strlen(string_32_a), string_32_e, strlen(string_32_e)) == true); \
        } \
        SECTION("same string same letters same case longer than 32") { \
            REQUIRE(utils_string_casecmp_eq_32##SUFFIX(string_50_f, strlen(string_50_f), string_50_f, strlen(string_50_f)) == true); \
        } \
        SECTION("different string same letters same case longer than 32") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_50_f, strlen(string_50_f), string_50_g, strlen(string_50_g)) == false); \
        } \
        SECTION("different string same letters same case different length longer than 32") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_50_f, strlen(string_50_f), string_60_h, strlen(string_60_h)) == false); \
        } \
    }

TEST_CASE("utils_string.c", "[utils_string]") {
#if defined(__x86_64__)
#if CACHEGRAND_CMAKE_CONFIG_HOST_HAS_AVX2 == 1
    TEST_UTILS_STRING_PLATFORM_DEPENDENT(_avx2)
#endif
#endif
    TEST_UTILS_STRING_PLATFORM_DEPENDENT(_sw)
    TEST_UTILS_STRING_PLATFORM_DEPENDENT()
}
