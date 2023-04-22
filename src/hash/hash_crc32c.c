/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"

#include "hash/hash_crc32c.h"

uint32_t hash_crc32c(
        const char* data,
        size_t data_len,
        uint32_t seed)
__attribute__ ((ifunc ("hash_crc32c_resolve")));

static void *hash_crc32c_resolve(void) {
    LOG_DI("Selecting optimal hash_crc32c");

#if defined(__x86_64__)
    __builtin_cpu_init();
    LOG_DI("CPU FOUND: %s", "X64");
    LOG_DI("> HAS SSE4.2: %s", __builtin_cpu_supports("sse4.2") ? "yes" : "no");

    if (__builtin_cpu_supports("sse4.2")) {
        LOG_DI("Selecting SSE4.2");

        return HASH_CRC32C(sse42);
    }
#endif

    LOG_DI("No optimization available, selecting software implementation");

    return HASH_CRC32C(sw);
}
