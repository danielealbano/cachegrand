#ifndef CACHEGRAND_NETWORK_CHANNEL_TLS_H
#define CACHEGRAND_NETWORK_CHANNEL_TLS_H

#ifdef __cplusplus
extern "C" {
#endif

int network_channel_tls_send_internal_mbed(
        void *context,
        const unsigned char *buffer,
        size_t buffer_length);

int network_channel_tls_receive_internal_mbed(
        void *context,
        unsigned char *buffer,
        size_t buffer_length);

bool network_channel_tls_ktls_supports_mbedtls_cipher_suite(
        network_channel_t *network_channel);

bool network_channel_tls_init(
        network_channel_t *network_channel);

void network_channel_tls_free(
        network_channel_t *network_channel);

bool network_channel_tls_handshake(
        network_channel_t *network_channel);

bool network_channel_tls_has_peer_certificate(
        network_channel_t *network_channel);

bool network_channel_tls_peer_certificate_get_cn(
        network_channel_t *network_channel,
        const char **cn,
        size_t *cn_length);

bool network_channel_tls_setup_ktls_tx_rx(
        network_channel_t *network_channel,
        int tx_or_rx);

bool network_channel_tls_setup_ktls(
        network_channel_t *network_channel);

void network_channel_tls_set_config(
        network_channel_t *network_channel,
        void *config);

bool network_channel_tls_get_config(
        network_channel_t *network_channel);

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

void network_channel_tls_set_mbedtls(
        network_channel_t *network_channel,
        bool mbedtls);

bool network_channel_tls_uses_mbedtls(
        network_channel_t *network_channel);

bool network_channel_tls_is_handshake_completed(
        network_channel_t *network_channel);

void network_channel_tls_set_handshake_completed(
        network_channel_t *network_channel,
        bool handshake_completed);

bool network_channel_tls_shutdown(
        network_channel_t *network_channel);

void network_channel_tls_free(
        network_channel_t *network_channel);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_CHANNEL_TLS_H
