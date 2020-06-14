#ifndef CACHEGRAND_MISC_H
#define CACHEGRAND_MISC_H

#ifdef DEBUG
#ifdef NDEBUG
#error "Can't define at the same time DEBUG and NDEBUG"
#endif
#endif

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#endif //CACHEGRAND_MISC_H
