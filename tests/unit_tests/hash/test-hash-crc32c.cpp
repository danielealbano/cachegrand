/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include "misc.h"
#include "hash/hash_crc32c.h"

char test_key_1[] = "test key 1";
size_t test_key_1_len = 10;
char test_key_2[26076];
size_t test_key_2_len = 26076;

#define TEST_HASH_CRC32C_PLATFORM_DEPENDENT(SUFFIX) \
    for(size_t i = 0; i < test_key_2_len; i++) { \
        test_key_2[i] = i & 0xFF; \
    } \
    SECTION("hash_crc32c" STRINGIZE(SUFFIX)) { \
        SECTION("valid crc32 - short key - aligned") { \
            REQUIRE(hash_crc32c##SUFFIX(test_key_1, test_key_1_len, 0x0000002AU) == 184302627); \
        } \
        SECTION("valid crc32 - short key - unaligned") { \
            REQUIRE(hash_crc32c##SUFFIX(test_key_1 + 1, test_key_1_len - 2, 0x0000002AU) == 1263559083); \
        } \
        SECTION("valid crc32 - long key - aligned") { \
            REQUIRE(hash_crc32c##SUFFIX(test_key_2, test_key_2_len, 0x0000002AU) == 1418798321); \
        } \
        SECTION("valid crc32 - long key - unaligned") { \
            REQUIRE(hash_crc32c##SUFFIX(test_key_2 + 1, test_key_2_len - 2, 0x0000002AU) == 90191070); \
        } \
        SECTION("seed 0") { \
            REQUIRE(hash_crc32c##SUFFIX(test_key_1, test_key_1_len, 0x00000000U) == 3252784223); \
        } \
        SECTION("seed 0xFFFFFFFF") { \
            REQUIRE(hash_crc32c##SUFFIX(test_key_1, test_key_1_len, 0xFFFFFFFFU) == 3720577995); \
        } \
    }

TEST_CASE("hash/hash_crc32c.c", "[hash][hash_crc32c]") {
#if defined(__x86_64__)
#if CACHEGRAND_CMAKE_CONFIG_HOST_HAS_SSE42 == 1
    TEST_HASH_CRC32C_PLATFORM_DEPENDENT(_sse42)
#endif
#endif
    TEST_HASH_CRC32C_PLATFORM_DEPENDENT(_sw)
    TEST_HASH_CRC32C_PLATFORM_DEPENDENT()
}
