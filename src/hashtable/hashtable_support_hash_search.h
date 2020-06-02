#ifndef CACHEGRAND_HASHTABLE_SUPPORT_HASH_SEARCH_H
#define CACHEGRAND_HASHTABLE_SUPPORT_HASH_SEARCH_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int8_t (*hashtable_support_hash_search_fp_t)(uint32_t hash, uint32_t* hashes);

extern int8_t hashtable_support_hash_search_avx2(uint32_t hash, uint32_t* hashes);
extern int8_t hashtable_support_hash_search_avx(uint32_t hash, uint32_t* hashes);
extern int8_t hashtable_support_hash_search_loop(uint32_t hash, uint32_t* hashes);

void hashtable_support_hash_search_select_instruction_set();

hashtable_support_hash_search_fp_t hashtable_support_hash_search;

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_HASH_SEARCH_H
