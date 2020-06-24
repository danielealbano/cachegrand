#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "exttypes.h"
#include "log.h"

#include "hash/hash_crc32c.h"

uint32_t hash_crc32c(
        const char* data,
        size_t data_len,
        uint32_t seed)
__attribute__ ((ifunc ("hash_crc32c_resolve")));

static void *hash_crc32c_resolve(void)
{
    __builtin_cpu_init();

    LOG_DI("Selecting optimal hash_crc32c");

#if defined(__x86_64__)
    LOG_DI("CPU FOUND: %s", "X64");
    LOG_DI("> HAS AVX2: %s", __builtin_cpu_supports("avx2") ? "yes" : "no");

    if (__builtin_cpu_supports("sse4.2")) {
        LOG_DI("Selecting AVX2");

        return HASH_CRC32C(sse42);
    }
#endif

    LOG_DI("No optimization available, selecting software implementation");

    return HASH_CRC32C(sw);
}
