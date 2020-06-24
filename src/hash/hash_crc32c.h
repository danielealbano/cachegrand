#ifndef CACHEGRAND_HASH_CRC32C_H
#define CACHEGRAND_HASH_CRC32C_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASH_CRC32C(METHOD) \
    hash_crc32c_##METHOD

#define HASH_CRC32C_SIGNATURE(METHOD) \
    extern uint32_t HASH_CRC32C(METHOD)( \
            const char* data, \
            size_t data_len, \
            uint32_t seed);

HASH_CRC32C_SIGNATURE(sw);
HASH_CRC32C_SIGNATURE(sse42);

uint32_t hash_crc32c(
        const char* data,
        size_t data_len,
        uint32_t seed);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASH_CRC32C_H



