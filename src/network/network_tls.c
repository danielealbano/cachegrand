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
#include <error.h>
#include <errno.h>
#include <arpa/inet.h>
#include <linux/tls.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/version.h>

#include "misc.h"
#include "exttypes.h"
#include "random.h"
#include "xalloc.h"
#include "log/log.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "support/simple_file_io.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/io/network_io_common_tls.h"
#include "module/module.h"
#include "config.h"
#include "network/channel/network_channel.h"
#include "network.h"

#include "network_tls_mbedtls.h"
#include "network_tls.h"

#define TAG "network_tls"

bool network_tls_is_ulp_tls_supported_internal() {
    bool result = true;
    struct sockaddr_in server_addr = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
    };
    struct sockaddr_in client_addr = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
    };

    // Create the sockets
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Try to find a free port for the server to bind to and listen temporarily
    int attempts = 0;
    int max_attempts = 100;
    do {
        // Generate a random port between 10000 and UINT16_MAX
        server_addr.sin_port = htons((random_generate() % (UINT16_MAX - 10000)) + 10000);
    } while(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0 &&
            attempts++ < max_attempts);

    // If we have reached the max attempts, we have failed
    if (attempts >= max_attempts) {
        result = false;
    }

    // Listen on the socket but do not start any accept, rely on backlog set to 1 to let the connection go through
    // unaccepted
    if (result) {
        if (listen(server_fd, 1) != 0) {
            result = false;
        }
    }

    // Connect to the server
    client_addr.sin_port = server_addr.sin_port;
    if (result) {
        if (connect(client_fd, (struct sockaddr *)&client_addr, sizeof(client_addr)) != 0) {
            result = false;
        }
    }

    // Try to enable the ULP TLS on the client fd
    if (result) {
        result = network_io_common_tls_socket_set_ulp_tls(client_fd);
    }

    // Close the sockets
    close(server_fd);
    close(client_fd);

    return result;
}

bool network_tls_does_ulp_tls_support_mbedtls_cipher_suite(
        mbedtls_cipher_type_t cipher_type) {
    // Should check the kernel version to ensure that if MBEDTLS_CIPHER_CHACHA20_POLY1305 is requested, at least a
    // kernel version 5.11 is being used
    switch(cipher_type) {
        case MBEDTLS_CIPHER_AES_128_GCM:
        case MBEDTLS_CIPHER_AES_256_GCM:
        case MBEDTLS_CIPHER_AES_128_CCM:
#if defined(TLS_CIPHER_CHACHA20_POLY1305)
        case MBEDTLS_CIPHER_CHACHA20_POLY1305:
#endif
            return true;
        default:
            return false;
    }
}

int network_tls_min_version_config_to_mbed(
        config_module_network_tls_min_version_t version) {
    int res;
    switch (version) {
        default:
            LOG_W(TAG, "Unsupported TLS min version");
            res = -1;
            break;
        case CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_ANY:
        case CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_0:
            res = MBEDTLS_SSL_MINOR_VERSION_1;
            break;
        case CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_1:
            res = MBEDTLS_SSL_MINOR_VERSION_2;
            break;
        case CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_2:
            res = MBEDTLS_SSL_MINOR_VERSION_3;
            break;
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
        case CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_3:
            res = MBEDTLS_SSL_MINOR_VERSION_4;
            break;
#endif
    }

    return res;
}

int network_tls_max_version_config_to_mbed(
        config_module_network_tls_max_version_t version) {
    int res;

    switch (version) {
        default:
            LOG_W(TAG, "Unsupported TLS max version");
            res = -1;
            break;
        case CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_0:
            res = MBEDTLS_SSL_MINOR_VERSION_1;
            break;
        case CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_1:
            res = MBEDTLS_SSL_MINOR_VERSION_2;
            break;
        case CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_2:
            res = MBEDTLS_SSL_MINOR_VERSION_3;
            break;
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
        case CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_3:
            res = MBEDTLS_SSL_MINOR_VERSION_4;
            break;
#endif
        case CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_ANY:
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
            res = MBEDTLS_SSL_MINOR_VERSION_4;
            break;
#else
            res = MBEDTLS_SSL_MINOR_VERSION_3;
#endif
    }

    return res;
}

bool network_tls_load_certificate(
        mbedtls_x509_crt *certificate,
        char *path) {
    if (mbedtls_x509_crt_parse_file(certificate, path) != 0) {
        return false;
    }

    return true;
}

bool network_tls_load_private_key(
        mbedtls_pk_context *private_key,
        char *path) {
    // TODO: the password is not supported on purpose, the certificates should be managed via the Linux kernel-side
    //       keyrings as it's possible to set the keys and certificates to use via a process with the necessary
    //       permissions in place, it also would allow easily to refresh the certificate / private key upon rotation
    //       without the need to restart the process
    if (mbedtls_pk_parse_keyfile(private_key, path, NULL) != 0) {
        return false;
    }

    return true;
}

int *network_tls_build_cipher_suites_from_names(
        char **cipher_suite_name,
        unsigned int cipher_suites_names_count,
        size_t *cipher_suites_ids_size) {
    *cipher_suites_ids_size = 0;

    if (cipher_suites_names_count == 0) {
        return NULL;
    }

    // There might be over allocation in case of invalid cipher suites
    int *cipher_suites_ids = xalloc_alloc_zero(sizeof(int) * (cipher_suites_names_count + 1));

    int cipher_suite_id_index = 0;
    for(
            int cipher_suite_name_index = 0;
            cipher_suite_name_index < cipher_suites_names_count;
            cipher_suite_name_index++) {
        const mbedtls_ssl_ciphersuite_t *mbedtls_ssl_ciphersuite =
                mbedtls_ssl_ciphersuite_from_string(cipher_suite_name[cipher_suite_name_index]);

        // Skip the unknown cipher suites
        if (mbedtls_ssl_ciphersuite == NULL) {
            LOG_W(TAG, "Cipher suite <%s> unsupported or unknown, ignoring",
                  cipher_suite_name[cipher_suite_name_index]);
            continue;
        }

        // Skip all the non-tls ciphers
        if (
                mbedtls_ssl_ciphersuite->max_major_ver < 3 ||
                (mbedtls_ssl_ciphersuite->max_major_ver == 3 && mbedtls_ssl_ciphersuite->max_minor_ver == 0)) {
            LOG_W(TAG, "Cipher suite <%s> unsupported or unknown, ignoring", cipher_suite_name[cipher_suite_name_index]);
            continue;
        }

        cipher_suites_ids[cipher_suite_id_index] = mbedtls_ssl_ciphersuite->id;
        cipher_suite_id_index++;
    }

    // Check if all the ciphers specified are invalid
    if (cipher_suite_id_index == 0) {
        xalloc_free(cipher_suites_ids);
        return NULL;
    }

    *cipher_suites_ids_size = sizeof(int) * (cipher_suite_id_index + 1);

    return cipher_suites_ids;
}

network_tls_config_t *network_tls_config_init(
        char *certificate_path,
        char *private_key_path,
        char *ca_certificate_chain_path,
        config_module_network_tls_min_version_t tls_min_version,
        config_module_network_tls_max_version_t tls_max_version,
        int *cipher_suites,
        size_t cipher_suites_length,
        bool verify_client_certificate) {
    network_tls_config_t *network_tls_config = NULL;
    bool return_res = false;

    network_tls_config = xalloc_alloc(sizeof(network_tls_config_t) + cipher_suites_length);
    if (network_tls_config == NULL) {
        goto end;
    }

    // Initialize everything here, if network_tls_config != NULL at the end, if there is a failure, it will invoke the
    // free functions from mbedtls and therefore everything has to be init-ed already.
    mbedtls_entropy_init(&network_tls_config->entropy);
    mbedtls_ctr_drbg_init(&network_tls_config->ctr_drbg);
    mbedtls_ssl_config_init(&network_tls_config->config);
    mbedtls_x509_crt_init(&network_tls_config->server_cert);
    mbedtls_pk_init(&network_tls_config->server_key);

    if (mbedtls_ctr_drbg_seed(
            &network_tls_config->ctr_drbg,
            mbedtls_entropy_func,
            &network_tls_config->entropy,
            NULL,
            0) != 0) {
        goto end;
    }

    if (mbedtls_ssl_config_defaults(
            &network_tls_config->config,
            MBEDTLS_SSL_IS_SERVER,
            MBEDTLS_SSL_TRANSPORT_STREAM,
            MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        goto end;
    }

    mbedtls_ssl_conf_rng(
            &network_tls_config->config,
            mbedtls_ctr_drbg_random,
            &network_tls_config->ctr_drbg);

    if (!network_tls_load_certificate(
            &network_tls_config->server_cert,
            certificate_path)) {
        goto end;
    }

    if (!network_tls_load_private_key(
            &network_tls_config->server_key,
            private_key_path)) {
        goto end;
    }

    if (mbedtls_ssl_conf_own_cert(
            &network_tls_config->config,
            &network_tls_config->server_cert,
            &network_tls_config->server_key) != 0) {
        goto end;
    }

    if (ca_certificate_chain_path) {
        if (!network_tls_load_certificate(
                &network_tls_config->server_ca_cert_chain,
                ca_certificate_chain_path)) {
            goto end;
        }

        mbedtls_ssl_conf_ca_chain(
                &network_tls_config->config,
                &network_tls_config->server_ca_cert_chain,
                NULL);
    }

    if (cipher_suites != NULL) {
        // Copy the cipher suites in network_tls_config
        memcpy(network_tls_config->cipher_suites, cipher_suites, cipher_suites_length);
        mbedtls_ssl_conf_ciphersuites(
                &network_tls_config->config,
                network_tls_config->cipher_suites);
    }

    mbedtls_ssl_conf_min_version(
            &network_tls_config->config,
            MBEDTLS_SSL_MAJOR_VERSION_3,
            network_tls_min_version_config_to_mbed(tls_min_version));

    // Set the TLS max version
    if (tls_max_version != CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_ANY) {
        mbedtls_ssl_conf_max_version(
                &network_tls_config->config,
                MBEDTLS_SSL_MAJOR_VERSION_3,
                network_tls_max_version_config_to_mbed(tls_max_version));
    }

    // Disable renegotiation & legacy renegotiation as in general regeneration is unsecure, it's not supported by kTLS
    // and it has even been removed from TLS 1.3
    mbedtls_ssl_conf_renegotiation(
            &network_tls_config->config,
            MBEDTLS_SSL_RENEGOTIATION_DISABLED);
    mbedtls_ssl_conf_legacy_renegotiation(
            &network_tls_config->config,
            MBEDTLS_SSL_LEGACY_NO_RENEGOTIATION);

    // If the client certificate verification is not required, then set the mutual authentication to optional, needed
    // to access the peer certificate information if passed by the client
    mbedtls_ssl_conf_authmode(
            &network_tls_config->config,
            verify_client_certificate
                ? MBEDTLS_SSL_VERIFY_REQUIRED
                : MBEDTLS_SSL_VERIFY_OPTIONAL);

    return_res = true;

end:
    if (!return_res && network_tls_config) {
        mbedtls_pk_free(&network_tls_config->server_key);
        mbedtls_x509_crt_free(&network_tls_config->server_cert);
        mbedtls_ctr_drbg_free(&network_tls_config->ctr_drbg);
        mbedtls_entropy_free(&network_tls_config->entropy);
        mbedtls_ssl_config_free(&network_tls_config->config);

        xalloc_free(network_tls_config);
        network_tls_config = NULL;
    }

    return network_tls_config;
}

void network_tls_config_free(
        network_tls_config_t *network_tls_config) {
    if (network_tls_config == NULL) {
        return;
    }

    mbedtls_pk_free(&network_tls_config->server_key);
    mbedtls_x509_crt_free(&network_tls_config->server_cert);
    mbedtls_ctr_drbg_free(&network_tls_config->ctr_drbg);
    mbedtls_entropy_free(&network_tls_config->entropy);
    mbedtls_ssl_config_free(&network_tls_config->config);

    xalloc_free(network_tls_config);
}

void network_tls_close_internal(
        network_channel_t *channel) {
    int32_t res;
    while((res = mbedtls_ssl_close_notify(channel->tls.context)) < 0) {
        if (res != MBEDTLS_ERR_SSL_WANT_READ && res != MBEDTLS_ERR_SSL_WANT_WRITE) {
            break;
        }
    }
}

network_op_result_t network_tls_receive_internal(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length,
        size_t *received_length) {
    int32_t res;
    *received_length = 0;
    if (channel->status == NETWORK_CHANNEL_STATUS_CLOSED) {
        return NETWORK_OP_RESULT_CLOSE_SOCKET;
    }

    while((res = mbedtls_ssl_read(
            channel->tls.context,
            (unsigned char*)buffer,
            buffer_length)) <= 0) {
        if (res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        }

        if (res == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || res == MBEDTLS_ERR_NET_CONN_RESET || res == 0) {
            LOG_D(
                    TAG,
                    "[FD:%5d][RECV] The client <%s> closed the connection",
                    channel->fd,
                    channel->address.str);

            return NETWORK_OP_RESULT_CLOSE_SOCKET;
        } else if (res == -ECANCELED) {
            LOG_V(
                    TAG,
                    "[FD:%5d][ERROR CLIENT] Send timeout to client <%s>",
                    channel->fd,
                    channel->address.str);
            return NETWORK_OP_RESULT_ERROR;
        } else {
            char errbuf[256] = { 0 };
            mbedtls_strerror(res, errbuf, sizeof(errbuf) - 1);

            LOG_V(
                    TAG,
                    "[FD:%5d][ERROR CLIENT] Error <%s (%d)> from client <%s>",
                    channel->fd,
                    errbuf,
                    -res,
                    channel->address.str);

            return NETWORK_OP_RESULT_ERROR;
        }
    }

    LOG_D(
            TAG,
            "[FD:%5d][RECV] Received <%u> bytes from client <%s>",
            channel->fd,
            res,
            channel->address.str);

    *received_length = res;

    return NETWORK_OP_RESULT_OK;
}

network_op_result_t network_tls_send_direct_internal(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length,
        size_t *sent_length) {
    int32_t res;

    if (channel->status == NETWORK_CHANNEL_STATUS_CLOSED) {
        return NETWORK_OP_RESULT_CLOSE_SOCKET;
    }

    while((res = mbedtls_ssl_write(
            channel->tls.context,
            (unsigned char*)buffer,
            buffer_length)) <= 0) {
        if (res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        }

        if (res == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || res == MBEDTLS_ERR_NET_CONN_RESET || res == 0) {
            LOG_D(
                    TAG,
                    "[FD:%5d][RECV] The client <%s> closed the connection",
                    channel->fd,
                    channel->address.str);

            return NETWORK_OP_RESULT_CLOSE_SOCKET;
        } else if (res == -ECANCELED) {
            LOG_V(
                    TAG,
                    "[FD:%5d][ERROR CLIENT] Send timeout to client <%s>",
                    channel->fd,
                    channel->address.str);
            return NETWORK_OP_RESULT_ERROR;
        } else {
            char errbuf[256] = { 0 };
            mbedtls_strerror(res, errbuf, sizeof(errbuf) - 1);

            LOG_V(
                    TAG,
                    "[FD:%5d][ERROR CLIENT] Error <%s (%d)> from client <%s>",
                    channel->fd,
                    errbuf,
                    -res,
                    channel->address.str);

            return NETWORK_OP_RESULT_ERROR;
        }
    }

    *sent_length = res;

    return NETWORK_OP_RESULT_OK;
}

char* network_tls_mbedtls_version() {
    return MBEDTLS_VERSION_STRING_FULL;
}

network_tls_mbedtls_cipher_suite_info_t *network_tls_mbedtls_get_all_cipher_suites_info() {
    int count, index;
    network_tls_mbedtls_cipher_suite_info_t *cipher_suites;
    const int *mbedtls_cipher_suites_ids = mbedtls_ssl_list_ciphersuites();

    // Count the cipher suites supported by mbedtls that can be used to allocate the necessary memory
    count = 0;
    for(
            const int *mbedtls_cipher_suite_id = mbedtls_cipher_suites_ids;
            *mbedtls_cipher_suite_id;
            mbedtls_cipher_suite_id++) {
        const mbedtls_ssl_ciphersuite_t *mbedtls_ssl_ciphersuite =
                mbedtls_ssl_ciphersuite_from_id(*mbedtls_cipher_suite_id);

        // Skip all the non-tls ciphers
        if (
                mbedtls_ssl_ciphersuite->max_major_ver < 3 ||
                (mbedtls_ssl_ciphersuite->max_major_ver == 3 && mbedtls_ssl_ciphersuite->max_minor_ver == 0)) {
            continue;
        }

        count++;
    }

    // Fills up the data to be returned
    cipher_suites = xalloc_alloc_zero(sizeof(network_tls_mbedtls_cipher_suite_info_t) * (count + 1));
    index = 0;
    for(
            const int *mbedtls_cipher_suite_id = mbedtls_cipher_suites_ids;
            *mbedtls_cipher_suite_id;
            mbedtls_cipher_suite_id++ && index++) {
        const mbedtls_ssl_ciphersuite_t *mbedtls_ssl_ciphersuite =
                mbedtls_ssl_ciphersuite_from_id(*mbedtls_cipher_suite_id);

        // Skip all the non-tls ciphers
        if (
                mbedtls_ssl_ciphersuite->max_major_ver < 3 ||
                (mbedtls_ssl_ciphersuite->max_major_ver == 3 && mbedtls_ssl_ciphersuite->max_minor_ver == 0)) {
            continue;
        }

        // Copy the name
        cipher_suites[index].name = mbedtls_ssl_ciphersuite->name;

        // Check if can use kernel offloading
        cipher_suites[index].offloading =
                network_tls_does_ulp_tls_support_mbedtls_cipher_suite(mbedtls_ssl_ciphersuite->cipher);

        // Set the min version for the cipher
        if (mbedtls_ssl_ciphersuite->min_minor_ver <= 1) {
            cipher_suites[index].min_version = CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_0;
        } else if (mbedtls_ssl_ciphersuite->min_minor_ver == 2) {
            cipher_suites[index].min_version = CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_1;
        } else if (mbedtls_ssl_ciphersuite->min_minor_ver == 3) {
            cipher_suites[index].min_version = CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_2;
        } else if (mbedtls_ssl_ciphersuite->min_minor_ver == 4) {
            cipher_suites[index].min_version = CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_3;
        }

        // Set the max version for the cipher
        if (mbedtls_ssl_ciphersuite->max_minor_ver == 1) {
            cipher_suites[index].max_version = CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_0;
        } else if (mbedtls_ssl_ciphersuite->max_minor_ver == 2) {
            cipher_suites[index].max_version = CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_1;
        } else if (mbedtls_ssl_ciphersuite->max_minor_ver == 3) {
            cipher_suites[index].max_version = CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_2;
        } else if (mbedtls_ssl_ciphersuite->max_minor_ver == 4) {
            cipher_suites[index].max_version = CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_3;
        }
    }

    return cipher_suites;
}

char *network_tls_min_version_to_string(
        config_module_network_tls_min_version_t version) {
    switch(version) {
        case CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_0:
            return "TLS 1.0";
        case CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_1:
            return "TLS 1.1";
        case CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_2:
            return "TLS 1.2";
        case CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_3:
            return "TLS 1.3";
        default:
        case CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_ANY:
            return "any";
    }
}

char *network_tls_max_version_to_string(
        config_module_network_tls_max_version_t version) {
    switch(version) {
        case CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_0:
            return "TLS 1.0";
        case CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_1:
            return "TLS 1.1";
        case CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_2:
            return "TLS 1.2";
        case CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_3:
            return "TLS 1.3";
        default:
        case CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_ANY:
            return "any";
    }
}
