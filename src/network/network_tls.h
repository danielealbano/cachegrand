#ifndef CACHEGRAND_NETWORK_TLS_H
#define CACHEGRAND_NETWORK_TLS_H

#ifdef __cplusplus
extern "C" {
#endif

#define NETWORK_TLS_PROC_SYS_NET_IPV4_TCP_AVAILABLE_ULP "/proc/sys/net/ipv4/tcp_available_ulp"

#define NETWORK_TLS_AES_128_CIPHERS \
        MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, \
        MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, \
        MBEDTLS_TLS_DHE_RSA_WITH_AES_128_GCM_SHA256, \
        MBEDTLS_TLS_DHE_PSK_WITH_AES_128_GCM_SHA256, \
        0x0

typedef struct network_tls_config network_tls_config_t;
struct network_tls_config {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_config config;
    mbedtls_x509_crt srvcert;
    mbedtls_pk_context srvkey;
};

bool network_tls_is_ulp_supported();

int network_channel_tls_send_internal_mbed(
        void *context,
        const unsigned char *buffer,
        size_t buffer_length);

int network_channel_tls_receive_internal_mbed(
        void *context,
        unsigned char *buffer,
        size_t buffer_length);

bool network_channel_tls_init(
        network_channel_t *network_channel,
        network_tls_config_t *network_tls_config);

void network_channel_tls_set_enabled(
        network_channel_t *network_channel,
        bool enabled);

bool network_channel_tls_is_enabled(
        network_channel_t *network_channel);

void network_channel_tls_set_ktls(
        network_channel_t *network_channel,
        bool ktls);

bool network_channel_tls_uses_ktls(
        network_channel_t *network_channel);

bool network_channel_tls_shutdown(
        network_channel_t *network_channel);

bool network_channel_tls_free(
        network_channel_t *network_channel);

bool network_tls_load_certificate(
        mbedtls_x509_crt *certificate,
        char *path);

bool network_tls_load_private_key(
        mbedtls_pk_context *private_key,
        char *path);

network_tls_config_t *network_tls_config_init(
        char *certificate_path,
        char *private_key_path,
        int chiper_suites[]);

void network_tls_config_free(
        network_tls_config_t *network_tls_config);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_TLS_H
