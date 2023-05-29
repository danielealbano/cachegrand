/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include <linux/tls.h>

#include <mbedtls/aes.h>
#include <mbedtls/chacha20.h>
#include <mbedtls/chachapoly.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/gcm.h>
#include <mbedtls/ccm.h>
#include <mbedtls/poly1305.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/oid.h>
#include <mbedtls/ssl_internal.h>

#include "misc.h"
#include "exttypes.h"
#include "xalloc.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "config.h"
#include "fiber/fiber.h"
#include "log/log.h"
#include "support/simple_file_io.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/io/network_io_common_tls.h"
#include "network/channel/network_channel.h"
#include "network/network.h"
#include "network/network_tls_mbedtls.h"
#include "network/network_tls.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/network/worker_network_op.h"

#include "network_channel_tls.h"

#define TAG "network_channel"

void chacha20_u32_to_bytes_le(uint32_t v, uint8_t* out) {
    out[0] = (uint8_t)(v);
    out[1] = (uint8_t)(v >> 8);
    out[2] = (uint8_t)(v >> 16);
    out[3] = (uint8_t)(v >> 24);
}

int network_channel_tls_send_internal_mbed(
        void *context,
        const unsigned char *buffer,
        size_t buffer_length) {
    return worker_op_network_send(
            context,
            (char *)buffer,
            buffer_length);
}

int network_channel_tls_receive_internal_mbed(
        void *context,
        unsigned char *buffer,
        size_t buffer_length) {
    network_channel_t *channel = context;
    if (likely(network_channel_tls_is_handshake_completed(channel))) {
        return worker_op_network_receive(
                context,
                (char *)buffer,
                buffer_length);
    } else {
        return worker_op_network_receive_timeout(
                context,
                (char *)buffer,
                buffer_length,
                500);
    }
}

bool network_channel_tls_ktls_supports_mbedtls_cipher_suite(
        network_channel_t *network_channel) {
    mbedtls_ssl_context *ssl_context = network_channel->tls.context;

    const mbedtls_ssl_ciphersuite_t *mbedtls_ssl_ciphersuite =
            mbedtls_ssl_ciphersuite_from_id(ssl_context->session->ciphersuite);

    return network_tls_does_ulp_tls_support_mbedtls_cipher_suite(
            mbedtls_ssl_ciphersuite->cipher);
}

bool network_channel_tls_init(
        network_channel_t *network_channel) {
    bool result_res = false;

    network_channel->tls.context = ffma_mem_alloc(sizeof(mbedtls_ssl_context));
    mbedtls_ssl_init(network_channel->tls.context);

    if (mbedtls_ssl_setup(
            network_channel->tls.context,
            network_channel->tls.config) != 0) {
        LOG_E(TAG, "Failed to setup the ssl session");
        goto end;
    }

    // It's not necessary to handle the timeout here, it's handled at the lower level by the network interfaces
    mbedtls_ssl_set_bio(
            network_channel->tls.context,
            &network_channel->fd,
            network_channel_tls_send_internal_mbed,
            network_channel_tls_receive_internal_mbed,
            NULL);

    result_res = true;

end:

    if (!result_res && network_channel->tls.context) {
        mbedtls_ssl_free(network_channel->tls.context);
        ffma_mem_free(network_channel->tls.context);

        network_channel->tls.context = NULL;
    }

    return result_res;
}

bool network_channel_tls_handshake(
        network_channel_t *network_channel) {
    char err_buffer[256] = { 0 };
    bool exit = false;
    bool return_res = false;

    do {
        int res;
        switch ((res = mbedtls_ssl_handshake(network_channel->tls.context))) {
            case MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS:
            case MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS:
            case MBEDTLS_ERR_SSL_WANT_READ:
            case MBEDTLS_ERR_SSL_WANT_WRITE:
                break;

            case 0:
                // TODO: network_flush currently does nothing but shortly data buffering for the sends will be
                //       implemented and to avoid performance penalties or ssl handshake problems it will be necessary
                //       to flush the buffer here therefore a network_flush has been added even if it does nothing
                network_flush_send_buffer(network_channel); // lgtm [cpp/useless-expression]
                return_res = true;
                exit = true;
                break;

            default:
                mbedtls_strerror(res, err_buffer, sizeof(err_buffer) - 1);
                LOG_V(TAG, "Failed to perform the TLS handshake because <%s>", err_buffer);
                exit = true;
        }
    } while(!exit);

    network_channel_tls_set_handshake_completed(network_channel, return_res);

    return return_res;
}

bool network_channel_tls_has_peer_certificate(
        network_channel_t *network_channel) {
    mbedtls_ssl_context *session_context = (mbedtls_ssl_context*)network_channel->tls.context;
    mbedtls_x509_crt *peer_cert = session_context->session->peer_cert;
    return peer_cert != NULL;
}

bool network_channel_tls_peer_certificate_get_cn(
        network_channel_t *network_channel,
        const char **cn,
        size_t *cn_length) {
    mbedtls_ssl_context *session_context = (mbedtls_ssl_context*)network_channel->tls.context;
    mbedtls_x509_crt *peer_cert = session_context->session->peer_cert;

    // To get the peer certificate verify has to be set to optional or required
    if (peer_cert == NULL) {
        return false;
    }

    // Iterate over the subject names to find the object id for the CN, the id is 3 bytes long and the value is
    // "\x55\x04\x03" (MBEDTLS_OID_AT_CN)
    mbedtls_x509_name *name = &peer_cert->subject;
    do {
        if (name->oid.len != 3 || strncmp((const char*)(name->oid.p), MBEDTLS_OID_AT_CN, 3) != 0) {
            continue;
        }

        *cn = (const char*)name->val.p;
        *cn_length = name->val.len;
        return true;
    } while((name = name->next) != NULL);

    return false;
}

bool network_channel_tls_setup_ktls_tx_rx(
        network_channel_t *network_channel,
        int tx_or_rx) {
    uint8_t *salt, *iv, *rec_seq;
    void *cipher_context;
    char chacha20_key[32] = { 0 };
    char chacha20_nonce[12] = { 0 };
    mbedtls_aes_context *aes_context;
    mbedtls_chacha20_context *chacha20_context;
    size_t crypto_info_length ;

    network_io_common_tls_crypto_info_t crypto_info = { 0 };
    mbedtls_ssl_context *ssl_context = network_channel->tls.context;
    const mbedtls_ssl_ciphersuite_t *mbedtls_ssl_ciphersuite =
            mbedtls_ssl_ciphersuite_from_id(ssl_context->session->ciphersuite);

    if (tx_or_rx == 0) {
        salt = ssl_context->transform->iv_enc;
        rec_seq = ssl_context->in_ctr;
    } else {
        salt = ssl_context->transform->iv_dec;
        rec_seq = ssl_context->cur_out_ctr;
    }
    iv = salt + 4;

    if (tx_or_rx == 0) {
        cipher_context = ssl_context->transform->cipher_ctx_enc.cipher_ctx;
    } else {
        cipher_context = ssl_context->transform->cipher_ctx_dec.cipher_ctx;
    }

    if (
            mbedtls_ssl_ciphersuite->cipher == MBEDTLS_CIPHER_AES_128_GCM ||
            mbedtls_ssl_ciphersuite->cipher == MBEDTLS_CIPHER_AES_256_GCM) {
        aes_context = ((mbedtls_gcm_context*)cipher_context)->cipher_ctx.cipher_ctx;
    } else if (mbedtls_ssl_ciphersuite->cipher == MBEDTLS_CIPHER_AES_128_CCM) {
        aes_context = ((mbedtls_ccm_context*)cipher_context)->cipher_ctx.cipher_ctx;
#if defined(TLS_CIPHER_CHACHA20_POLY1305)
    } else if (mbedtls_ssl_ciphersuite->cipher == MBEDTLS_CIPHER_CHACHA20_POLY1305) {
        chacha20_context = &((mbedtls_chachapoly_context*)cipher_context)->chacha20_ctx;
#endif
    }

    int tls_version =
            ((mbedtls_ssl_ciphersuite->min_major_ver & 0xFF) << 8) +
            (mbedtls_ssl_ciphersuite->min_major_ver & 0xFF);

    switch(mbedtls_ssl_ciphersuite->cipher) {
        case MBEDTLS_CIPHER_AES_128_GCM:
            crypto_info.tls12_crypto_info_aes_gcm_128.info.version = tls_version,
                    crypto_info.tls12_crypto_info_aes_gcm_128.info.cipher_type = TLS_CIPHER_AES_GCM_128;
            memcpy(crypto_info.tls12_crypto_info_aes_gcm_128.iv, iv, TLS_CIPHER_AES_GCM_128_IV_SIZE);
            memcpy(crypto_info.tls12_crypto_info_aes_gcm_128.rec_seq, rec_seq, TLS_CIPHER_AES_GCM_128_REC_SEQ_SIZE);
            memcpy(crypto_info.tls12_crypto_info_aes_gcm_128.key, aes_context->rk, TLS_CIPHER_AES_GCM_128_KEY_SIZE);
            memcpy(crypto_info.tls12_crypto_info_aes_gcm_128.salt, salt, TLS_CIPHER_AES_GCM_128_SALT_SIZE);
            crypto_info_length = sizeof(crypto_info.tls12_crypto_info_aes_gcm_128);
            break;

        case MBEDTLS_CIPHER_AES_256_GCM:
            crypto_info.tls12_crypto_info_aes_gcm_256.info.version = tls_version,
                    crypto_info.tls12_crypto_info_aes_gcm_256.info.cipher_type = TLS_CIPHER_AES_GCM_256;
            memcpy(crypto_info.tls12_crypto_info_aes_gcm_256.iv, iv, TLS_CIPHER_AES_GCM_256_IV_SIZE);
            memcpy(crypto_info.tls12_crypto_info_aes_gcm_256.rec_seq, rec_seq, TLS_CIPHER_AES_GCM_256_REC_SEQ_SIZE);
            memcpy(crypto_info.tls12_crypto_info_aes_gcm_256.key, aes_context->rk, TLS_CIPHER_AES_GCM_256_KEY_SIZE);
            memcpy(crypto_info.tls12_crypto_info_aes_gcm_256.salt, salt, TLS_CIPHER_AES_GCM_256_SALT_SIZE);
            crypto_info_length = sizeof(crypto_info.tls12_crypto_info_aes_gcm_256);
            break;

        case MBEDTLS_CIPHER_AES_128_CCM:
            crypto_info.tls12_crypto_info_aes_ccm_128.info.version = tls_version,
                    crypto_info.tls12_crypto_info_aes_ccm_128.info.cipher_type = TLS_CIPHER_AES_CCM_128;
            memcpy(crypto_info.tls12_crypto_info_aes_ccm_128.iv, iv, TLS_CIPHER_AES_CCM_128_IV_SIZE);
            memcpy(crypto_info.tls12_crypto_info_aes_ccm_128.rec_seq, rec_seq, TLS_CIPHER_AES_CCM_128_REC_SEQ_SIZE);
            memcpy(crypto_info.tls12_crypto_info_aes_ccm_128.key, aes_context->rk, TLS_CIPHER_AES_CCM_128_KEY_SIZE);
            memcpy(crypto_info.tls12_crypto_info_aes_ccm_128.salt, salt, TLS_CIPHER_AES_CCM_128_SALT_SIZE);
            crypto_info_length = sizeof(crypto_info.tls12_crypto_info_aes_ccm_128);
            break;

#if defined(TLS_CIPHER_CHACHA20_POLY1305)
        case MBEDTLS_CIPHER_CHACHA20_POLY1305:
            // Fetch the iv from the state (8 uint32, 32 bytes)
            for(int index = 0; index < 8; index++) {
#if defined(__x86_64__) || defined(__aarch64__)
                chacha20_u32_to_bytes_le(
                        chacha20_context->state[index + 4],
                        (uint8_t *)chacha20_key + (index * sizeof(uint32_t)));
#else
#error architecture not supported
#endif
            }

            // Fetch the iv from the state (3 uint32, 12 bytes)
            for(int index = 0; index < 3; index++) {
#if defined(__x86_64__) || defined(__aarch64__)
                chacha20_u32_to_bytes_le(
                        chacha20_context->state[index + 13],
                        (uint8_t *)chacha20_nonce + (index * sizeof(uint32_t)));
#else
#error architecture not supported
#endif
            }

            crypto_info.tls12_crypto_info_chacha20_poly1305.info.version = tls_version,
                    crypto_info.tls12_crypto_info_chacha20_poly1305.info.cipher_type = TLS_CIPHER_CHACHA20_POLY1305;
            memcpy(crypto_info.tls12_crypto_info_chacha20_poly1305.iv, chacha20_nonce, TLS_CIPHER_CHACHA20_POLY1305_IV_SIZE);
            memcpy(crypto_info.tls12_crypto_info_chacha20_poly1305.rec_seq, rec_seq, TLS_CIPHER_CHACHA20_POLY1305_REC_SEQ_SIZE);
            memcpy(crypto_info.tls12_crypto_info_chacha20_poly1305.key, chacha20_key, TLS_CIPHER_CHACHA20_POLY1305_KEY_SIZE);
            memcpy(crypto_info.tls12_crypto_info_chacha20_poly1305.salt, salt, TLS_CIPHER_CHACHA20_POLY1305_SALT_SIZE);
            crypto_info_length = sizeof(crypto_info.tls12_crypto_info_chacha20_poly1305);
            break;
#endif

        default:
            return false;
    }

    if (tx_or_rx == 0) {
        return network_io_common_tls_socket_set_tls_tx(
                network_channel->fd,
                &crypto_info,
                crypto_info_length);
    } else {
        return network_io_common_tls_socket_set_tls_rx(
                network_channel->fd,
                &crypto_info,
                crypto_info_length);
    }
}

bool network_channel_tls_setup_ktls(
        network_channel_t *network_channel) {
    if (!network_io_common_tls_socket_set_ulp_tls(network_channel->fd)) {
        return false;
    }

    if (!network_channel_tls_setup_ktls_tx_rx(network_channel, 0)) {
        LOG_V(TAG, "Failed to setup kTLS, unable to setup the tx offloading");
        return false;
    }

    if (!network_channel_tls_setup_ktls_tx_rx(network_channel, 1)) {
        LOG_V(TAG, "Failed to setup kTLS, unable to setup the rx offloading");
        return false;
    }

    return true;
}

void network_channel_tls_set_config(
        network_channel_t *network_channel,
        void *config) {
    network_channel->tls.config = config;
}

bool network_channel_tls_get_config(
        network_channel_t *network_channel) {
    return network_channel->tls.config;
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

void network_channel_tls_set_mbedtls(
        network_channel_t *network_channel,
        bool mbedtls) {
    network_channel->tls.mbedtls = mbedtls;
}

bool network_channel_tls_uses_mbedtls(
        network_channel_t *network_channel) {
    return network_channel->tls.mbedtls;
}

bool network_channel_tls_shutdown(
        network_channel_t *network_channel) {
    return mbedtls_ssl_close_notify(network_channel->tls.context) == 0;
}

void network_channel_tls_free(
        network_channel_t *network_channel) {
    if (network_channel->tls.context == NULL) {
        return;
    }

    mbedtls_ssl_free(network_channel->tls.context);
    ffma_mem_free(network_channel->tls.context);
    network_channel->tls.context = NULL;
}
