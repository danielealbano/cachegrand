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
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <error.h>
#include <math.h>
#include <xalloc.h>

#include "misc.h"

#include "utils_string.h"

IFUNC_WRAPPER_RESOLVE(UTILS_STRING_NAME_IFUNC(cmp_eq_32)) {
#if defined(__x86_64__)
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx2")) {
        return UTILS_STRING_NAME_IMPL(cmp_eq_32, avx2);
    }
#else
#warning "missing optimized string function for the current architecture"
#endif

    return UTILS_STRING_NAME_IMPL(cmp_eq_32, sw);
}

bool IFUNC_WRAPPER(UTILS_STRING_NAME_IFUNC(cmp_eq_32),
                   (const char *a, size_t a_len, const char *b, size_t b_len));


IFUNC_WRAPPER_RESOLVE(UTILS_STRING_NAME_IFUNC(casecmp_eq_32)) {
#if defined(__x86_64__)
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx2")) {
        return UTILS_STRING_NAME_IMPL(casecmp_eq_32, avx2);
    }
#else
#warning "missing optimized string function for the current architecture"
#endif

    return UTILS_STRING_NAME_IMPL(casecmp_eq_32, sw);
}

bool IFUNC_WRAPPER(UTILS_STRING_NAME_IFUNC(casecmp_eq_32),
                   (const char *a, size_t a_len, const char *b, size_t b_len));

// Derived from https://www.codeproject.com/Articles/5163931/Fast-String-Matching-with-Wildcards-Globs-and-Giti
uint32_t utils_string_utf8_decode_char(
        char *string,
        size_t string_length,
        size_t *char_length) {
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

// Derived from https://www.codeproject.com/Articles/5163931/Fast-String-Matching-with-Wildcards-Globs-and-Giti
bool utils_string_glob_match(
        char *string,
        size_t string_length,
        char *pattern,
        size_t pattern_length) {
    bool matched, reverse;
    char *string_backup = NULL, *pattern_backup = NULL;
    size_t string_length_backup = 0, pattern_length_backup = 0;

    if (unlikely(pattern_length == 0 || string_length == 0)) {
        return false;
    }

    if (unlikely(pattern_length == 1 && (*pattern == '*' || *pattern == '?'))) {
        return true;
    }

    while (string_length > 0) {
        switch (*pattern) {
            case '*':
                string_backup = string;
                pattern_backup = ++pattern;
                string_length_backup = string_length;
                pattern_length_backup = --pattern_length;
                continue;

            case '?':
                string++;
                pattern++;
                string_length--;
                pattern_length--;
                continue;

            case '[':
                if (pattern_length <= 1) {
                    break;
                }

                pattern++;
                pattern_length--;
                reverse = false;

                if (*pattern == '^') {
                    reverse = true;
                    pattern++;
                    pattern_length--;
                }

                matched = false;
                int prev_char = -1;
                while(pattern_length > 0 && *pattern != ']') {
                    switch(*pattern) {
                        case '-':
                            if (prev_char == -1) {
                                continue;
                            }

                            pattern++;
                            pattern_length--;
                            if (pattern_length == 0) {
                                continue;
                            }

                            if (!matched) {
                                if (*string >= prev_char && *string <= *pattern) {
                                    matched = true;
                                }
                            }
                            break;

                        case '\\':
                            pattern++;
                            pattern_length--;
                            if (pattern_length == 0) {
                                continue;
                            }

                        default:
                            if (!matched) {
                                if (*string == *pattern) {
                                    matched = true;
                                }
                            }

                            prev_char = (int)*pattern;
                            pattern++;
                            pattern_length--;
                            break;
                    }
                }

                if (*pattern != ']') {
                    break;
                }

                if (reverse) {
                    matched = matched ? false : true;
                }

                if (!matched) {
                    break;
                }

                pattern++;
                pattern_length--;
                string++;
                string_length--;
                continue;

            case '\\':
                pattern++;
                pattern_length--;

                if (pattern_length == 0) {
                    break;
                }

            default:
                if (*pattern != *string) {
                    break;
                }

                string++;
                pattern++;
                string_length--;
                pattern_length--;
                continue;
        }

        if (pattern_backup == NULL) {
            return false;
        }

        string = ++string_backup;
        pattern = pattern_backup;
        string_length = --string_length_backup;
        pattern_length = pattern_length_backup;
    }

    while (pattern_length > 0 && *pattern == '*') {
        pattern++;
        pattern_length--;
    }

    return pattern_length == 0 && string_length == 0 ? true : false;
}

int64_t utils_string_to_int64(
        char *string,
        size_t string_length,
        bool *invalid) {
    uint64_t number = 0;
    int sign_multiplier = 1;
    *invalid = false;

    if (unlikely(string_length == 0)) {
        return 0;
    }

    if (unlikely(string_length > UTILS_STRING_INT64_MIN_STR_LENGTH)) {
        *invalid = true;
        return 0;
    }

    if (*string == '-') {
        sign_multiplier = -1;
        string_length--;
        string++;
    }

    while(likely(string_length--)) {
        // Handle non-numeric chars
        if (unlikely(!isdigit(*string))) {
            *invalid = true;
            return 0;
        }

        uint64_t prev_number = number;
        number = (number * 10) + (*string - '0');
        string++;

        // Handle numbers that are too long and overflow
        if (unlikely(number < prev_number)) {
            *invalid = true;
            return 0;
        }
    }

    if (unlikely(number > ((uint64_t)INT64_MAX) + (sign_multiplier == -1 ? 1 : 0))) {
        *invalid = true;
        return 0;
    }

    return (int64_t)number * sign_multiplier;
}

long double utils_string_to_long_double(
        char *string,
        size_t string_length,
        bool *invalid) {
    long double number;
    char *end_ptr = NULL;
    bool allocated_new_buffer = false;
    char static_buffer[128] = { 0 };
    char *buffer = static_buffer;
    size_t buffer_length = sizeof(static_buffer);

    if (unlikely(string_length == 0)) {
        return 0;
    }

    if (unlikely(string_length > buffer_length - 1)) {
        allocated_new_buffer = true;
        buffer_length = string_length + 1;
        buffer = xalloc_alloc(buffer_length);
        buffer[string_length] = 0;
    }

    memcpy(buffer, string, string_length);

    errno = 0;
    number = strtold(buffer, &end_ptr);
    if (unlikely(
            end_ptr - buffer != string_length ||
            errno == ERANGE ||
            isnan(number))) {
        *invalid = true;
        number = 0;
    }

    if (unlikely(allocated_new_buffer)) {
        xalloc_free(buffer);
    }

    return number;
}
