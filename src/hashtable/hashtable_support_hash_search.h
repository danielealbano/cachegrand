#ifndef CACHEGRAND_HASHTABLE_SUPPORT_HASH_SEARCH_H
#define CACHEGRAND_HASHTABLE_SUPPORT_HASH_SEARCH_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int8_t (*hashtable_support_hash_search_fp_t)(uint32_t hash, volatile uint32_t* hashes);

typedef struct simd_search_fp_map_entry simd_search_fp_map_entry_t;
struct simd_search_fp_map_entry {
    const char* feature_name;
    int8_t (*fp)(uint32_t hash, volatile uint32_t* hashes);
};

#if defined(__AVX512F__)
int8_t hashtable_support_hash_search_avx512(uint32_t hash, volatile uint32_t* hashes);
#endif

#if defined(__AVX2__)
int8_t hashtable_support_hash_search_avx2(uint32_t hash, volatile uint32_t* hashes);
#endif

#if defined(__SSE4_2__)
int8_t hashtable_support_hash_search_sse42(uint32_t hash, volatile uint32_t* hashes);
#endif

int8_t hashtable_support_hash_search_loop(uint32_t hash, volatile uint32_t* hashes);
void hashtable_support_hash_search_select_instruction_set();

hashtable_support_hash_search_fp_t hashtable_support_hash_search;

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_HASH_SEARCH_H
