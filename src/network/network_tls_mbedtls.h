#ifndef CACHEGRAND_NETWORK_TLS_MBEDTLS_H
#define CACHEGRAND_NETWORK_TLS_MBEDTLS_H

#ifdef __cplusplus
extern "C" {
#endif

#define NETWORK_TLS_AES_128_CIPHERS \
            { \
                MBEDTLS_TLS_RSA_WITH_AES_128_GCM_SHA256, \
                MBEDTLS_TLS_DHE_RSA_WITH_AES_128_GCM_SHA256, \
                MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256, \
                MBEDTLS_TLS_DHE_PSK_WITH_AES_128_GCM_SHA256, \
                MBEDTLS_TLS_RSA_PSK_WITH_AES_128_GCM_SHA256, \
                MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, \
                MBEDTLS_TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256, \
                MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, \
                MBEDTLS_TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256, \
                0x0 \
            }

typedef struct network_tls_config network_tls_config_t;
struct network_tls_config {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_config config;
    mbedtls_x509_crt server_cert;
    mbedtls_pk_context server_key;
    mbedtls_x509_crt server_ca_cert_chain;
    int cipher_suites[];
};

bool network_tls_does_ulp_tls_support_mbedtls_cipher_suite(
        mbedtls_cipher_type_t cipher_suite_id);

int network_tls_min_version_config_to_mbed(
        config_module_network_tls_min_version_t version);

int network_tls_max_version_config_to_mbed(
        config_module_network_tls_max_version_t version);

bool network_tls_load_certificate(
        mbedtls_x509_crt *certificate,
        char *path);

bool network_tls_load_private_key(
        mbedtls_pk_context *private_key,
        char *path);

network_op_result_t network_tls_receive_internal(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length,
        size_t *received_length);

network_op_result_t network_tls_send_direct_internal(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length,
        size_t *sent_length);

void network_tls_close_internal(
        network_channel_t *channel);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_TLS_MBEDTLS_H
