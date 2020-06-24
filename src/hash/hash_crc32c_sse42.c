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

// Depend on HASH_CRC32C_POLY
#define LONG 8192
#define LONGx1 "8192"
#define LONGx2 "16384"
#define SHORT 256
#define SHORTx1 "256"
#define SHORTx2 "512"

static uint32_t crc32c_sse42_tables_long[4][256];
static uint32_t crc32c_sse42_tables_short[4][256];

__attribute__((constructor))
static void crc32c_sse42_init() {
    hash_crc32c_common_zeros(crc32c_sse42_tables_long, LONG);
    hash_crc32c_common_zeros(crc32c_sse42_tables_short, SHORT);
}

uint32_t crc32c_sse42(
        const char* data,
        size_t data_len,
        uint32_t seed) {
    const unsigned char *next = (const unsigned char *)data;
    const unsigned char *end;
    uint64_t crc0, crc1, crc2;      /* need to be 64 bits for crc32q */

    /* pre-process the crc */
    crc0 = seed ^ 0xffffffff;

    /* compute the crc for up to seven leading bytes to bring the data pointer
       to an eight-byte boundary */
    while (data_len && ((uintptr_t)next & 7) != 0) {
        __asm__("crc32b\t" "(%1), %0"
        : "=r"(crc0)
        : "r"(next), "0"(crc0));
        next++;
        data_len--;
    }

    /* compute the crc on sets of LONG*3 bytes, executing three independent crc
       instructions, each on LONG bytes -- this is optimized for the Nehalem,
       Westmere, Sandy Bridge, and Ivy Bridge architectures, which have a
       throughput of one crc per cycle, but a latency of three cycles */
    while (data_len >= LONG * 3) {
        crc1 = 0;
        crc2 = 0;
        end = next + LONG;
        do {
            __asm__("crc32q\t" "(%3), %0\n\t"
                    "crc32q\t" LONGx1 "(%3), %1\n\t"
                                      "crc32q\t" LONGx2 "(%3), %2"
            : "=r"(crc0), "=r"(crc1), "=r"(crc2)
            : "r"(next), "0"(crc0), "1"(crc1), "2"(crc2));
            next += 8;
        } while (next < end);
        crc0 = hash_crc32c_common_shift(crc32c_sse42_tables_long, crc0) ^ crc1;
        crc0 = hash_crc32c_common_shift(crc32c_sse42_tables_long, crc0) ^ crc2;
        next += LONG*2;
        data_len -= LONG * 3;
    }

    /* do the same thing, but now on SHORT*3 blocks for the remaining data less
       than a LONG*3 block */
    while (data_len >= SHORT * 3) {
        crc1 = 0;
        crc2 = 0;
        end = next + SHORT;
        do {
            __asm__("crc32q\t" "(%3), %0\n\t"
                    "crc32q\t" SHORTx1 "(%3), %1\n\t"
                                       "crc32q\t" SHORTx2 "(%3), %2"
            : "=r"(crc0), "=r"(crc1), "=r"(crc2)
            : "r"(next), "0"(crc0), "1"(crc1), "2"(crc2));
            next += 8;
        } while (next < end);
        crc0 = hash_crc32c_common_shift(crc32c_sse42_tables_short, crc0) ^ crc1;
        crc0 = hash_crc32c_common_shift(crc32c_sse42_tables_short, crc0) ^ crc2;
        next += SHORT*2;
        data_len -= SHORT * 3;
    }

    /* compute the crc on the remaining eight-byte units less than a SHORT*3
       block */
    end = next + (data_len - (data_len & 7));
    while (next < end) {
        __asm__("crc32q\t" "(%1), %0"
        : "=r"(crc0)
        : "r"(next), "0"(crc0));
        next += 8;
    }
    data_len &= 7;

    /* compute the crc for up to seven trailing bytes */
    while (data_len) {
        __asm__("crc32b\t" "(%1), %0"
        : "=r"(crc0)
        : "r"(next), "0"(crc0));
        next++;
        data_len--;
    }

    /* return a post-processed crc */
    return (uint32_t)crc0 ^ 0xffffffff;
}
