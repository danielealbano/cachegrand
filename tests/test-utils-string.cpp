/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <string.h>
#include <catch2/catch.hpp>

#include "misc.h"
#include "utils_string.h"

const char string_a[32] = "this is a string";
const char string_b[32] = "this is string a";
const char string_c[32] = "string with a different length";
const char string_d[32] = "this is a string but different";
const char string_e[32] = "THIS IS A STRING";

#define TEST_UTILS_STRING_PLATFORM_DEPENDENT(SUFFIX) \
    SECTION("utils_string_cmp_eq_32" STRINGIZE(SUFFIX)) { \
        SECTION("same string lower case") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_a, strlen(string_a), string_a, strlen(string_a)) == true); \
        } \
        SECTION("same string upper case") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_e, strlen(string_e), string_e, strlen(string_e)) == true); \
        } \
        SECTION("different string with different length") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_a, strlen(string_a), string_c, strlen(string_c)) == false); \
        } \
        SECTION("different string with same length") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_b, strlen(string_b), string_c, strlen(string_c)) == false); \
        } \
        SECTION("different string same start and same length") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_a, strlen(string_a), string_b, strlen(string_b)) == false); \
        } \
        SECTION("differt string same letters different case") { \
            REQUIRE(utils_string_cmp_eq_32##SUFFIX(string_a, strlen(string_a), string_e, strlen(string_e)) == false); \
        } \
    } \
    SECTION("utils_string_casecmp_eq_32" STRINGIZE(SUFFIX)) { \
        SECTION("same string lower case") { \
            REQUIRE(utils_string_casecmp_eq_32##SUFFIX(string_a, strlen(string_a), string_a, strlen(string_a)) == true); \
        } \
        SECTION("same string upper case") { \
            REQUIRE(utils_string_casecmp_eq_32##SUFFIX(string_e, strlen(string_e), string_e, strlen(string_e)) == true); \
        } \
        SECTION("different string with different length") { \
            REQUIRE(utils_string_casecmp_eq_32##SUFFIX(string_a, strlen(string_a), string_c, strlen(string_c)) == false); \
        } \
        SECTION("different string with same length") { \
            REQUIRE(utils_string_casecmp_eq_32##SUFFIX(string_b, strlen(string_b), string_c, strlen(string_c)) == false); \
        } \
        SECTION("different string same start and same length") { \
            REQUIRE(utils_string_casecmp_eq_32##SUFFIX(string_a, strlen(string_a), string_b, strlen(string_b)) == false); \
        } \
        SECTION("different string same letters different case") { \
            REQUIRE(utils_string_casecmp_eq_32##SUFFIX(string_a, strlen(string_a), string_e, strlen(string_e)) == true); \
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
