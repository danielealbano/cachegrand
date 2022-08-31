#ifndef CACHEGRAND_UTILS_STRING_H
#define CACHEGRAND_UTILS_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

#define UTILS_STRING_INT64_MIN_STR_LENGTH (strlen("-9223372036854775808"))

// These double wrappers are used to allow the usage of nested macros
#define IFUNC_WRAPPER2(NAME, ARGS) NAME ARGS __attribute__ ((ifunc (#NAME "_resolve")));
#define IFUNC_WRAPPER(NAME, ARGS) IFUNC_WRAPPER2(NAME, ARGS)

#define IFUNC_WRAPPER_RESOLVE2(NAME) static void* NAME##_resolve(void)
#define IFUNC_WRAPPER_RESOLVE(NAME) IFUNC_WRAPPER_RESOLVE2(NAME)

#define UTILS_STRING_NAME_IFUNC(NAME) utils_string_##NAME
#define UTILS_STRING_SIGNATURE_IFUNC(NAME, ARGS) UTILS_STRING_NAME_IFUNC(NAME) ARGS

#define UTILS_STRING_NAME_IMPL(NAME, METHOD) utils_string_##NAME##_##METHOD
#define UTILS_STRING_SIGNATURE_IMPL(NAME, METHOD, ARGS) UTILS_STRING_NAME_IMPL(NAME, METHOD) ARGS

bool UTILS_STRING_SIGNATURE_IFUNC(cmp_eq_32, (const char *a, size_t a_len, const char *b, size_t b_len));
bool UTILS_STRING_SIGNATURE_IMPL(cmp_eq_32, sw, (const char *a, size_t a_len, const char *b, size_t b_len));
bool UTILS_STRING_SIGNATURE_IMPL(cmp_eq_32, avx2, (const char *a, size_t a_len, const char *b, size_t b_len));
bool UTILS_STRING_SIGNATURE_IMPL(cmp_eq_32, avx2_internal, (const char *a, size_t a_len, const char *b, size_t b_len));

bool UTILS_STRING_SIGNATURE_IFUNC(casecmp_eq_32, (const char *a, size_t a_len, const char *b, size_t b_len));
bool UTILS_STRING_SIGNATURE_IMPL(casecmp_eq_32, sw, (const char *a, size_t a_len, const char *b, size_t b_len));
bool UTILS_STRING_SIGNATURE_IMPL(casecmp_eq_32, avx2, (const char *a, size_t a_len, const char *b, size_t b_len));
bool UTILS_STRING_SIGNATURE_IMPL(casecmp_eq_32, avx2_internal, (const char *a, size_t a_len, const char *b, size_t b_len));

uint32_t utils_string_utf8_decode_char(
        char *string,
        size_t string_length,
        size_t *char_length);

bool utils_string_glob_match(
        char *string,
        size_t string_length,
        char *pattern,
        size_t pattern_length);

int64_t utils_string_to_int64(
        char *string,
        size_t string_length,
        bool *invalid);

long double utils_string_to_long_double(
        char *string,
        size_t string_length,
        bool *invalid);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_UTILS_STRING_H
