/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "utils_string.h"

IFUNC_WRAPPER_RESOLVE(UTILS_STRING_NAME_IFUNC(cmp_eq_32)) {
    __builtin_cpu_init();
#if defined(__x86_64__)
    if (__builtin_cpu_supports("avx2")) {
        return UTILS_STRING_NAME_IMPL(cmp_eq_32, avx2);
    }
#endif

    return UTILS_STRING_NAME_IMPL(cmp_eq_32, sw);
}

bool IFUNC_WRAPPER(UTILS_STRING_NAME_IFUNC(cmp_eq_32),
                   (const char *a, size_t a_len, const char *b, size_t b_len));


IFUNC_WRAPPER_RESOLVE(UTILS_STRING_NAME_IFUNC(casecmp_eq_32)) {
    __builtin_cpu_init();
#if defined(__x86_64__)
    if (__builtin_cpu_supports("avx2")) {
        return UTILS_STRING_NAME_IMPL(casecmp_eq_32, avx2);
    }
#endif

    return UTILS_STRING_NAME_IMPL(casecmp_eq_32, sw);
}

bool IFUNC_WRAPPER(UTILS_STRING_NAME_IFUNC(casecmp_eq_32),
                   (const char *a, size_t a_len, const char *b, size_t b_len));

// Derived from https://www.codeproject.com/Articles/5163931/Fast-String-Matching-with-Wildcards-Globs-and-Giti
uint32_t utils_string_utf8_decode_char(char *string, size_t string_length, size_t *char_length) {
    *char_length = 0;
    int c1, c2, c3, c = (unsigned char)*string;

    if (string_length > 0) {
        string++;
        string_length--;
        (*char_length)++;
    }

    if (c < 0x80) {
        return c;
    }

    c1 = (unsigned char)*string;
    if (c < 0xC0 || (c == 0xC0 && c1 != 0x80) || c == 0xC1 || (c1 & 0xC0) != 0x80) {
        return 0xFFFD;
    }

    if (string_length > 0) {
        string++;
        string_length--;
        (*char_length)++;
    }

    c1 &= 0x3F;
    if (c < 0xE0) {
        return (((c & 0x1F) << 6) | c1);
    }

    c2 = (unsigned char)*string;
    if ((c == 0xE0 && c1 < 0x20) || (c2 & 0xC0) != 0x80) {
        return 0xFFFD;
    }

    if (string_length > 0) {
        string++;
        string_length--;
        (*char_length)++;
    }

    c2 &= 0x3F;
    if (c < 0xF0) {
        return (((c & 0x0F) << 12) | (c1 << 6) | c2);
    }

    c3 = (unsigned char)*string;

    if (string_length > 0) {
        string++;
        string_length--;
        (*char_length)++;
    }

    if ((c == 0xF0 && c1 < 0x10) || (c == 0xF4 && c1 >= 0x10) || c >= 0xF5 || (c3 & 0xC0) != 0x80) {
        return 0xFFFD;
    }

    return (((c & 0x07) << 18) | (c1 << 12) | (c2 << 6) | (c3 & 0x3F));
}
