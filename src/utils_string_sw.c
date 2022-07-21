/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "utils_string.h"
#include "misc.h"

bool UTILS_STRING_SIGNATURE_IMPL(cmp_eq_32, sw, (const char *a, size_t a_len, const char *b, size_t b_len)) {
    if (a_len != b_len) {
        return false;
    }

    return strncmp(a, b, MAX(a_len, b_len)) == 0;
}

bool UTILS_STRING_SIGNATURE_IMPL(casecmp_eq_32, sw, (const char *a, size_t a_len, const char *b, size_t b_len)) {
    if (a_len != b_len) {
        return false;
    }

    return strncasecmp(a, b, MAX(a_len, b_len)) == 0;
}
