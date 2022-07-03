/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
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
                   (const char a[32], size_t a_len, const char b[32], size_t b_len));


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
                   (const char a[32], size_t a_len, const char b[32], size_t b_len));
