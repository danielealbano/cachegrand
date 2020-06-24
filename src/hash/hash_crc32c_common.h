#ifndef CACHEGRAND_HASH_CRC32C_COMMON_H
#define CACHEGRAND_HASH_CRC32C_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASH_CRC32C_POLY 0x82f63b78


uint32_t hash_crc32c_common_gf2_matrix_times(
        uint32_t *mat,
        uint32_t vec);

void hash_crc32c_common_gf2_matrix_square(
        uint32_t *square,
        uint32_t *mat);

void hash_crc32c_common_zeros_op(
        uint32_t *even,
        size_t len);

void hash_crc32c_common_zeros(
        uint32_t zeros[][256],
        size_t len);

uint32_t hash_crc32c_common_shift(
        uint32_t zeros[][256],
        uint32_t crc);


#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASH_CRC32C_COMMON_H
