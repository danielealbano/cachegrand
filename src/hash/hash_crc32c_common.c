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

#include <stdlib.h>
#include <stdint.h>

#include "hash_crc32c_common.h"

uint32_t hash_crc32c_common_gf2_matrix_times(
        uint32_t *mat,
        uint32_t vec) {
    uint32_t sum;

    sum = 0;
    while (vec) {
        if (vec & 1u)
            sum ^= *mat;
        vec >>= 1u;
        mat++;
    }
    return sum;
}

void hash_crc32c_common_gf2_matrix_square(
        uint32_t *square,
        uint32_t *mat) {
    int n;

    for (n = 0; n < 32; n++)
        square[n] = hash_crc32c_common_gf2_matrix_times(mat, mat[n]);
}

void hash_crc32c_common_zeros_op(
        uint32_t *even,
        size_t len) {
    int n;
    uint32_t row;
    uint32_t odd[32];       /* odd-power-of-two zeros operator */

    /* put operator for one zero bit in odd */
    odd[0] = HASH_CRC32C_POLY;              /* CRC-32C polynomial */
    row = 1;
    for (n = 1; n < 32; n++) {
        odd[n] = row;
        row <<= 1u;
    }

    /* put operator for two zero bits in even */
    hash_crc32c_common_gf2_matrix_square(even, odd);

    /* put operator for four zero bits in odd */
    hash_crc32c_common_gf2_matrix_square(odd, even);

    /* first square will put the operator for one zero byte (eight zero bits),
       in even -- next square puts operator for two zero bytes in odd, and so
       on, until len has been rotated down to zero */
    do {
        hash_crc32c_common_gf2_matrix_square(even, odd);
        len >>= 1u;
        if (len == 0)
            return;
        hash_crc32c_common_gf2_matrix_square(odd, even);
        len >>= 1u;
    } while (len);

    /* answer ended up in odd -- copy to even */
    for (n = 0; n < 32; n++)
        even[n] = odd[n];
}

void hash_crc32c_common_zeros(
        uint32_t zeros[][256],
        size_t len)
{
    uint32_t n;
    uint32_t op[32];

    hash_crc32c_common_zeros_op(op, len);
    for (n = 0; n < 256; n++) {
        zeros[0][n] = hash_crc32c_common_gf2_matrix_times(op, n);
        zeros[1][n] = hash_crc32c_common_gf2_matrix_times(op, n << 8u);
        zeros[2][n] = hash_crc32c_common_gf2_matrix_times(op, n << 16u);
        zeros[3][n] = hash_crc32c_common_gf2_matrix_times(op, n << 24u);
    }
}

uint32_t hash_crc32c_common_shift(
        uint32_t zeros[][256],
        uint32_t crc)
{
    return zeros[0][crc & 0xffu] ^ zeros[1][(crc >> 8u) & 0xffu] ^
           zeros[2][(crc >> 16u) & 0xffu] ^ zeros[3][crc >> 24u];
}
