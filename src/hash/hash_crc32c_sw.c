/* crc32c.c -- compute CRC-32C using the Intel crc32 instruction
 * Copyright (C) 2013 Mark Adler
 * Version 1.1  1 Aug 2013  Mark Adler
 */

/*
  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler
  madler@alumni.caltech.edu
 */

/* Use hardware CRC instruction on Intel SSE 4.2 processors.  This computes a
   CRC-32C, *not* the CRC-32 used by Ethernet and zip, gzip, etc.  A software
   version is provided as a fall-back, as well as for speed comparisons. */

/* Version history:
   1.0  10 Feb 2013  First version
   1.1   1 Aug 2013  Correct comments on why three crc instructions in parallel
 */

/**
 * Code taken from
 * https://stackoverflow.com/questions/17645167/implementing-sse-4-2s-crc32c-in-software/17646775#17646775
 *
 * It has been refactored and split in 3 different files, the copyright notice as well as the original license has been
 * added to the 3 .c files.
 */

#include <stdint.h>
#include <stdlib.h>

#include "hash_crc32c_common.h"

static uint32_t crc32c_table[8][256];

__attribute__((constructor))
static void hash_crc32c_sw_init() {
    uint32_t n, crc, k;

    for (n = 0; n < 256; n++) {
        crc = n;
        crc = crc & 1u ? (crc >> 1u) ^ HASH_CRC32C_POLY : crc >> 1u;
        crc = crc & 1u ? (crc >> 1u) ^ HASH_CRC32C_POLY : crc >> 1u;
        crc = crc & 1u ? (crc >> 1u) ^ HASH_CRC32C_POLY : crc >> 1u;
        crc = crc & 1u ? (crc >> 1u) ^ HASH_CRC32C_POLY : crc >> 1u;
        crc = crc & 1u ? (crc >> 1u) ^ HASH_CRC32C_POLY : crc >> 1u;
        crc = crc & 1u ? (crc >> 1u) ^ HASH_CRC32C_POLY : crc >> 1u;
        crc = crc & 1u ? (crc >> 1u) ^ HASH_CRC32C_POLY : crc >> 1u;
        crc = crc & 1u ? (crc >> 1u) ^ HASH_CRC32C_POLY : crc >> 1u;
        crc32c_table[0][n] = crc;
    }

    for (n = 0; n < 256; n++) {
        crc = crc32c_table[0][n];
        for (k = 1; k < 8; k++) {
            crc = crc32c_table[0][crc & 0xffu] ^ (crc >> 8u);
            crc32c_table[k][n] = crc;
        }
    }
}

uint32_t hash_crc32c_sw(
        const char* data,
        size_t data_len,
        uint32_t seed) {
    const unsigned char *next = (const unsigned char *)data;
    uint64_t crc;

    crc = seed ^ 0xffffffff;
    while (data_len && ((uintptr_t)next & 7u) != 0) {
        crc = crc32c_table[0][(crc ^ *next++) & 0xffu] ^ (crc >> 8u);
        data_len--;
    }
    while (data_len >= 8) {
        crc ^= *(uint64_t *)next;
        crc = crc32c_table[7][crc & 0xffu] ^
              crc32c_table[6][(crc >> 8u) & 0xffu] ^
              crc32c_table[5][(crc >> 16u) & 0xffu] ^
              crc32c_table[4][(crc >> 24u) & 0xffu] ^
              crc32c_table[3][(crc >> 32u) & 0xffu] ^
              crc32c_table[2][(crc >> 40u) & 0xffu] ^
              crc32c_table[1][(crc >> 48u) & 0xffu] ^
              crc32c_table[0][crc >> 56u];
        next += 8;
        data_len -= 8;
    }
    while (data_len) {
        crc = crc32c_table[0][(crc ^ *next++) & 0xffu] ^ (crc >> 8u);
        data_len--;
    }
    return (uint32_t)crc ^ 0xffffffff;
}
