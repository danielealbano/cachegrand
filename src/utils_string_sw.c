#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "utils_string.h"
#include "misc.h"

bool UTILS_STRING_SIGNATURE_IMPL(cmp_eq_32, sw, (const char a[32], size_t a_len, const char b[32], size_t b_len)) {
    if (a_len != b_len) {
        return false;
    }

    return strncmp(a, b, max(a_len, b_len)) == 0;
}

bool UTILS_STRING_SIGNATURE_IMPL(casecmp_eq_32, sw, (const char a[32], size_t a_len, const char b[32], size_t b_len)) {
    if (a_len != b_len) {
        return false;
    }

    return strncasecmp(a, b, max(a_len, b_len)) == 0;
}
