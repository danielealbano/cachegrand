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

#define FUNCTION_STATIC(NAME, ...) \
    static void NAME () { \
        __VA_ARGS__ \
    }

#define FUNCTION_CTOR_DTOR(SECTION_TYPE, SECTION_TYPE_STR, NAME, FUNC_BODY) \
    static void CONCAT(NAME, SECTION_TYPE) (){ \
        FUNC_BODY \
    } \
    \
    void (*CONCAT(CONCAT(NAME, SECTION_TYPE), fp))(void) ELF_SECTION(SECTION_TYPE_STR) = \
        CONCAT(NAME, SECTION_TYPE); \

#define FUNCTION_CTOR(NAME, ...) FUNCTION_CTOR_DTOR(ctors, ".ctors", NAME, __VA_ARGS__)
#define FUNCTION_DTOR(NAME, ...) FUNCTION_CTOR_DTOR(dtors, ".dtors", NAME, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MISC_H
