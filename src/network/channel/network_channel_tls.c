#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include <linux/tls.h>

#include <mbedtls/aes.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/gcm.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_internal.h>

#include "misc.h"
#include "exttypes.h"
#include "xalloc.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "slab_allocator.h"
#include "config.h"
#include "support/simple_file_io.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/io/network_io_common_tls.h"
#include "network/channel/network_channel.h"
#include "network/network.h"
#include "network/network_tls.h"

#include "network_channel_tls.h"

int network_channel_tls_send_internal_mbed(
        void *context,
        const unsigned char *buffer,
        size_t buffer_length) {
    if (!network_send(
            context,
            (network_channel_buffer_data_t*)buffer,
            buffer_length)) {
        return -1;
    }

    return (int)buffer_length;
}

int network_channel_tls_receive_internal_mbed(
        void *context,
        unsigned char *buffer,
        size_t buffer_length) {
    size_t read_length = 0;
    if (network_receive(
            context,
            (network_channel_buffer_data_t*)buffer,
            buffer_length,
            &read_length) != NETWORK_OP_RESULT_OK) {
        return -1;
    }

    return (int)read_length;
}

bool network_channel_tls_init(
        network_channel_t *network_channel,
        network_tls_config_t *network_tls_config) {
    bool result_res = false;

    network_channel->tls.context = slab_allocator_mem_alloc(sizeof(mbedtls_ssl_context));
    mbedtls_ssl_init(network_channel->tls.context);

    if (mbedtls_ssl_setup(
            network_channel->tls.context,
            &network_tls_config->config) != 0) {
        goto end;
    }

    // It's not necessary to handle the timeout here, it's handled at the lower level by the network interfaces
    mbedtls_ssl_set_bio(
            network_channel->tls.context,
            network_channel,
            network_channel_tls_send_internal_mbed,
            network_channel_tls_receive_internal_mbed,
            NULL);

    result_res = true;

end:

    if (!result_res && network_channel->tls.context) {
        mbedtls_ssl_free(network_channel->tls.context);
        slab_allocator_free(network_channel->tls.context);

        network_channel->tls.context = NULL;
    }

    return result_res;
}

bool network_channel_tls_is_ktls_supported_cipher() {
    // TODO: not implemented
    return false;
}

bool network_channel_tls_setup_ktls_tx_rx(
        network_channel_t *network_channel,
        network_tls_config_t *network_tls_config,
        int tx_or_rx) {
    uint8_t *salt, *iv,  *rec_seq;
    mbedtls_gcm_context *gcm_context;

    mbedtls_ssl_context *ssl_context = network_channel->tls.context;

    // TODO: should actually check the encryption algorithm is AES and that it's using GCM

    if (tx_or_rx == 0) {
        salt = ssl_context->transform->iv_dec;
        rec_seq = ssl_context->in_ctr;
        gcm_context = ssl_context->transform->cipher_ctx_dec.cipher_ctx;
    } else {
        salt = ssl_context->transform->iv_enc;
        rec_seq = ssl_context->cur_out_ctr;
        gcm_context = ssl_context->transform->cipher_ctx_enc.cipher_ctx;
    }

    iv = salt + 4;
    mbedtls_aes_context *aes_context = gcm_context->cipher_ctx.cipher_ctx;

    // TODO: should be built dynamically based on the crypto algorithm available
    network_io_common_tls_crypto_info_t crypto_info = { 0 };
    crypto_info.tls12_crypto_info_aes_gcm_128.info.version = TLS_1_2_VERSION,
    crypto_info.tls12_crypto_info_aes_gcm_128.info.cipher_type = TLS_CIPHER_AES_GCM_128;
    memcpy(crypto_info.tls12_crypto_info_aes_gcm_128.iv, iv, TLS_CIPHER_AES_GCM_128_IV_SIZE);
    memcpy(crypto_info.tls12_crypto_info_aes_gcm_128.rec_seq, rec_seq, TLS_CIPHER_AES_GCM_128_REC_SEQ_SIZE);
    memcpy(crypto_info.tls12_crypto_info_aes_gcm_128.key, aes_context->rk, TLS_CIPHER_AES_GCM_128_KEY_SIZE);
    memcpy(crypto_info.tls12_crypto_info_aes_gcm_128.salt, salt, TLS_CIPHER_AES_GCM_128_SALT_SIZE);

    if (tx_or_rx == 0) {
        return network_io_common_tls_socket_set_tls_tx(
                network_channel->fd,
                &crypto_info,
                sizeof(crypto_info.tls12_crypto_info_aes_gcm_128));
    } else {
        return network_io_common_tls_socket_set_tls_rx(
                network_channel->fd,
                &crypto_info,
                sizeof(crypto_info.tls12_crypto_info_aes_gcm_128));
    }
}

bool network_channel_tls_setup_ktls(
        network_channel_t *network_channel,
        network_tls_config_t *network_tls_config) {
    if (!network_channel_tls_setup_ktls_tx_rx(
            network_channel,
            network_tls_config,
            0)) {
        return false;
    }

    if (!network_channel_tls_setup_ktls_tx_rx(
            network_channel,
            network_tls_config,
            1)) {
        return false;
    }

    return true;
}

void network_channel_tls_set_enabled(
        network_channel_t *network_channel,
        bool enabled) {
    network_channel->tls.enabled = enabled;
}

bool network_channel_tls_is_enabled(
        network_channel_t *network_channel) {
    return network_channel->tls.enabled;
}

void network_channel_tls_set_ktls(
        network_channel_t *network_channel,
        bool ktls) {
    network_channel->tls.ktls = ktls;
}

bool network_channel_tls_uses_ktls(
        network_channel_t *network_channel) {
    return network_channel->tls.ktls;
}

bool network_channel_tls_shutdown(
        network_channel_t *network_channel) {
    mbedtls_ssl_close_notify(network_channel->tls.context);
}

bool network_channel_tls_free(
        network_channel_t *network_channel) {
    mbedtls_ssl_free(network_channel->tls.context);
    network_channel->tls.context = false;
}