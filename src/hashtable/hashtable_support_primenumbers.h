#ifndef CACHEGRAND_HASHTABLE_SUPPORT_PRIMENUMBERS_H
#define CACHEGRAND_HASHTABLE_SUPPORT_PRIMENUMBERS_H

#ifdef __cplusplus
extern "C" {
#endif

// TODO: calculate the hashtable sizes that are able to fit in 2mb pages to maximize the consumption of the memory that
//       gets allocated by the system (the hugepages have to be rounded up to 2mb or 1gb, cachegrand is using 2mb)
#define HASHTABLE_PRIMENUMBERS_LIST \
    42U, /* not a prime number, but it's the answer */ \
    101U, \
    307U, \
    677U, \
    1523U, \
    3389U, \
    7639U, \
    17203U, \
    26813U, \
    40213U, \
    60353U, \
    90529U, \
    135799U, \
    203669U, \
    305581U, \
    458377U, \
    687581U, \
    1031399U, \
    1547101U, \
    2320651U, \
    5221501U, \
    7832021U, \
    11748391U, \
    17622551U, \
    26433887U, \
    39650833U, \
    59476253U, \
    89214403U, \
    133821599U, \
    200732527U, \
    301099033U, \
    451649113U, \
    677472127U, \
    1016208581U, \
    1524312899U, \
    2286469357U, \
    3429704039U, \
    4294967291U

#define HASHTABLE_PRIMENUMBERS_MAX      4294967291U

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
