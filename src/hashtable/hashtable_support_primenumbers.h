#ifndef CACHEGRAND_HASHTABLE_SUPPORT_PRIMENUMBERS_H
#define CACHEGRAND_HASHTABLE_SUPPORT_PRIMENUMBERS_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASHTABLE_PRIMENUMBERS_FOREACH(list, index, list_value, ...) { \
    hashtable_bucket_count_t list[] = { HASHTABLE_PRIMENUMBERS_LIST }; \
    for(uint64_t index = 0; index < HASHTABLE_PRIMENUMBERS_COUNT; index++) { \
        hashtable_bucket_count_t list_value = list[index]; \
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
