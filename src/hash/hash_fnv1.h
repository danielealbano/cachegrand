#ifndef CACHEGRAND_HASH_FNV1_H
#define CACHEGRAND_HASH_FNV1_H

#ifdef __cplusplus
extern "C" {
#endif

#define FNV1_32_INIT ((uint32_t)0x811c9dc5)

#define fnv_32_hash(buf, len) (fnv_32_hash_internal(buf, len, FNV1_32_INIT))
#define fnv_32_hash_ci(buf, len) (fnv_32_hash_ci_internal(buf, len, FNV1_32_INIT))

static inline __attribute__((always_inline)) uint32_t fnv_32_hash_internal(
        void *buf,
        uint16_t len,
        uint32_t hval) {
    char *cp = (char*)buf;
    while (len > 0) {
        char c = *cp++;
        hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
        hval ^= (uint32_t)c;
        len--;
    }

    return hval;
}

static inline __attribute__((always_inline)) uint32_t fnv_32_hash_ci_internal(
        void *buf,
        uint16_t len,
        uint32_t hval) {
    char *cp = (char*)buf;
    while (len > 0) {
        char c = *cp++;
        c = c >= 'A' && c <= 'Z' ? c | (char) 32 : c;

        hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
        hval ^= (uint32_t)c;
        len--;
    }

    return hval;
}

#ifdef __cplusplus
}
#endif

#endif // CACHEGRAND_HASH_FNV1_H
