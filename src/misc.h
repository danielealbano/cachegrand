#ifndef CACHEGRAND_MISC_H
#define CACHEGRAND_MISC_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
#ifdef NDEBUG
#error "Can't define at the same time DEBUG and NDEBUG"
#endif
#endif

#if DEBUG == 1 &&  __has_include(<valgrind/valgrind.h>)
#include <valgrind/valgrind.h>
#define HAS_VALGRIND 1
#else
#undef HAS_VALGRIND
#endif

/* gcc doesn't know _Thread_local from C11 yet */
#ifdef __GNUC__
#define thread_local __thread
#elif __STDC_VERSION__ >= 201112L
# define thread_local _Thread_local
#elif defined(_MSC_VER)
# define thread_local __declspec(thread)
#else
# error Cannot define thread_local
#endif

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

#define SIZEOF_MEMBER(type, member) (sizeof(((type *)0)->member))

#define ELF_SECTION( S ) __attribute__ ((section ( S )))

#define CONCAT_(a, b)   a ## _ ## b
#define CONCAT(a, b)    CONCAT_(a, b)

#define STRINGIZE_NX(a) #a
#define STRINGIZE(a)    STRINGIZE_NX(a)

#define WRAPFORINCLUDE(a)    <a>

#define ARRAY_SIZE(ARRAY) (sizeof(ARRAY) / sizeof((ARRAY)[0]))

#define MAX(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define MIN(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(!!(COND))*2-1]
#define COMPILE_TIME_ASSERT3(X,L) STATIC_ASSERT(X,static_assertion_at_line_##L)
#define COMPILE_TIME_ASSERT2(X,L) COMPILE_TIME_ASSERT3(X,L)
#define COMPILE_TIME_ASSERT(X) COMPILE_TIME_ASSERT2(X,__LINE__)

#define FUNCTION_CTOR_DTOR(SECTION_TYPE, SECTION_TYPE_STR, NAME, FUNC_BODY) \
    static void CONCAT(NAME, SECTION_TYPE) (){ \
        FUNC_BODY \
    } \
    \
    void (*CONCAT(CONCAT(NAME, SECTION_TYPE), fp))(void) ELF_SECTION(SECTION_TYPE_STR) = \
        CONCAT(NAME, SECTION_TYPE); \

#define FUNCTION_CTOR(NAME, ...) FUNCTION_CTOR_DTOR(ctors, ".init_array", NAME, __VA_ARGS__)
#define FUNCTION_DTOR(NAME, ...) FUNCTION_CTOR_DTOR(dtors, ".fini_array", NAME, __VA_ARGS__)

#if (BYTE_ORDER == BIG_ENDIAN)
#define int16_hton(v) (v)
#define int16_ntoh(v) (v)
#define int16_htole(v) htole16(v)
#define int16_letoh(v) le16toh(v)

#define int32_hton(v) (v)
#define int32_ntoh(v) (v)
#define int32_htole(v) htole32(v)
#define int32_letoh(v) le32toh(v)

#define int64_hton(v) (v)
#define int64_ntoh(v) (v)
#define int64_htole(v) htole64(v)
#define int64_letoh(v) le64toh(v)
#else
#define int16_hton(v) htobe16(v)
#define int16_ntoh(v) be16toh(v)
#define int16_htole(v) (v)
#define int16_letoh(v) (v)

#define int32_hton(v) htobe32(v)
#define int32_ntoh(v) be32toh(v)
#define int32_htole(v) (v)
#define int32_letoh(v) (v)

#define int64_hton(v) htobe64(v)
#define int64_ntoh(v) be64toh(v)
#define int64_htole(v) (v)
#define int64_letoh(v) (v)
#endif

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MISC_H
