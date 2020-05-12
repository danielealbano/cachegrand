#ifndef CACHEGRAND_HASHTABLE_SUPPORT_PRIMENUMBERS_H
#define CACHEGRAND_HASHTABLE_SUPPORT_PRIMENUMBERS_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASHTABLE_PRIMENUMBERS_FOREACH(list, index, ...) { \
    hashtable_bucket_index_t list[] = { HASHTABLE_PRIMENUMBERS_LIST }; \
    hashtable_bucket_index_t list_length = sizeof(list) / sizeof(list[0]); \
    _Pragma("GCC ivdep") _Pragma("GCC unroll(16)") \
    for(uint64_t index = 0; index < list_length; index++) { \
__VA_ARGS__ \
    } \
}

bool hashtable_support_primenumbers_valid(uint64_t number);

uint64_t hashtable_support_primenumbers_next(uint64_t number);

uint64_t hashtable_support_primenumbers_mod(uint64_t number, uint64_t prime);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_PRIMENUMBERS_H
