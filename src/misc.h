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

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

#define ELF_SECTION( S ) __attribute__ ((section ( S )))

#define concat_(a, b)   a ## _ ## b
#define concat(a, b)    concat_(a, b)

#define STRINGIZE_NX(a) #a
#define STRINGIZE(a)    STRINGIZE_NX(a)

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define FUNCTION_CTOR_DTOR(SECTION_TYPE, SECTION_TYPE_STR, NAME, FUNC_BODY) \
    static void concat(NAME, SECTION_TYPE) (){ \
        FUNC_BODY \
    } \
    \
    void (*concat(concat(NAME, SECTION_TYPE), fp))(void) ELF_SECTION(SECTION_TYPE_STR) = \
        concat(NAME, SECTION_TYPE); \

#define FUNCTION_CTOR(NAME, ...) FUNCTION_CTOR_DTOR(ctors, ".ctors", NAME, __VA_ARGS__)
#define FUNCTION_DTOR(NAME, ...) FUNCTION_CTOR_DTOR(dtors, ".dtors", NAME, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MISC_H
