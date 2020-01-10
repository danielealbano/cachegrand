#ifndef CACHEGRAND_HASHTABLE_SUPPORT_PRIMENUMBERS_H
#define CACHEGRAND_HASHTABLE_SUPPORT_PRIMENUMBERS_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASHTABLE_PRIMENUMBERS_MAX      5144556059U

bool hashtable_support_primenumbers_valid(uint64_t number);

uint64_t hashtable_support_primenumbers_next(uint64_t number);

uint64_t hashtable_support_primenumbers_mod(uint64_t number, uint64_t prime);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_PRIMENUMBERS_H
