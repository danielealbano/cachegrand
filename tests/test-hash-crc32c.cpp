#include "catch.hpp"

#include "misc.h"
#include "hash/hash_crc32c.h"

char test_key_1[] = "test key 1";
size_t test_key_1_len = 10;

#define TEST_HASH_CRC32C_PLATFORM_DEPENDENT(SUFFIX) \
    SECTION("hash_crc32c" STRINGIZE(SUFFIX)) { \
        SECTION("valid crc32") { \
            REQUIRE(hash_crc32c##SUFFIX(test_key_1, test_key_1_len, 0x0000002AU) == 184302627); \
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
