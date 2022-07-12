#ifndef CACHEGRAND_NETWORK_IO_COMMON_TLS_H
#define CACHEGRAND_NETWORK_IO_COMMON_TLS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tls12_crypto_info_aes_gcm_128 tls12_crypto_info_aes_gcm_128_t;
typedef struct tls12_crypto_info_aes_gcm_256 tls12_crypto_info_aes_gcm_256_t;
typedef struct tls12_crypto_info_aes_ccm_128 tls12_crypto_info_aes_ccm_128_t;
typedef struct tls12_crypto_info_chacha20_poly1305 tls12_crypto_info_chacha20_poly1305_t;

typedef union network_io_common_tls_crypto_info network_io_common_tls_crypto_info_t;
union network_io_common_tls_crypto_info {
    tls12_crypto_info_aes_gcm_128_t tls12_crypto_info_aes_gcm_128;
    tls12_crypto_info_aes_gcm_256_t tls12_crypto_info_aes_gcm_256;
    tls12_crypto_info_aes_ccm_128_t tls12_crypto_info_aes_ccm_128;
    tls12_crypto_info_chacha20_poly1305_t tls12_crypto_info_chacha20_poly1305;
};

bool network_io_common_tls_socket_set_ulp(
        network_io_common_fd_t fd,
        char *ulp);

bool network_io_common_tls_socket_set_tls_rx(
        network_io_common_fd_t fd,
        network_io_common_tls_crypto_info_t *val,
        size_t length);

bool network_io_common_tls_socket_set_tls_tx(
        network_io_common_fd_t fd,
        network_io_common_tls_crypto_info_t *val,
        size_t length);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_IO_COMMON_TLS_H
