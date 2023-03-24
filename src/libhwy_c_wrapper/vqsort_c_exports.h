#ifndef CACHEGRAND_VQSORT_C_EXPORTS_H
#define CACHEGRAND_VQSORT_C_EXPORTS_H

#ifdef __cplusplus
extern "C" {
#endif

struct vqsort_kv32 {
  uint32_t value;
  uint32_t key;
} __attribute__((__aligned__(8)));
typedef struct vqsort_kv32 vqsort_kv32_t;

struct vqsort_kv64 {
  uint64_t value;
  uint64_t key;
} __attribute__((__aligned__(16)));
typedef struct vqsort_kv64 vqsort_kv64_t;

typedef unsigned __int128 uint128_t;

void vqsort_kv64_asc(
        vqsort_kv32_t* keys,
        size_t n);

void vqsort_kv64_desc(
        vqsort_kv32_t* keys,
        size_t n);

void vqsort_kv128_asc(
        vqsort_kv64_t* keys,
        size_t n);

void vqsort_kv128_desc(
        vqsort_kv64_t* keys,
        size_t n);

void vqsort_u16_asc(
        uint16_t* keys,
        size_t n);

void vqsort_u16_desc(
        uint16_t* keys,
        size_t n);

void vqsort_u32_asc(
        uint32_t* keys,
        size_t n);

void vqsort_u32_desc(
        uint32_t* keys,
        size_t n);

void vqsort_u64_asc(
        uint64_t* keys,
        size_t n);

void vqsort_u64_desc(
        uint64_t* keys,
        size_t n);

void vqsort_u128_asc(
        uint128_t* keys,
        size_t n);

void vqsort_u128_desc(
        uint128_t* keys,
        size_t n);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_VQSORT_C_EXPORTS_H
