#ifndef CACHEGRAND_MISC_H
#define CACHEGRAND_MISC_H

#ifdef DEBUG
#ifdef NDEBUG
#error "Can't define at the same time DEBUG and NDEBUG"
#endif
#endif

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

#define concat_(a, b)   a ## _ ## b
#define concat(a, b)    concat_(a, b)

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#endif //CACHEGRAND_MISC_H
