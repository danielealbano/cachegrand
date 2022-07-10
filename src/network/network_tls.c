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
#include "support/simple_file_io.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/io/network_io_common_tls.h"
#include "network/protocol/network_protocol.h"
#include "config.h"
#include "network/channel/network_channel.h"

#include "network_tls.h"

bool network_tls_is_ulp_supported() {
    char buffer[256];

    if (simple_file_io_read(
            NETWORK_TLS_PROC_SYS_NET_IPV4_TCP_AVAILABLE_ULP,
            buffer,
            sizeof(buffer))) {
        return false;
    }

    if (strstr(buffer, "tls")) {
        return true;
    }

    return false;
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

network_tls_config_t *network_tls_config_init(
        char *certificate_path,
        char *private_key_path,
        int chiper_suites[]) {
    network_tls_config_t *network_tls_config = NULL;
    bool return_res = false;

    network_tls_config = slab_allocator_mem_alloc(sizeof(network_tls_config_t));
    if (network_tls_config == NULL) {
        goto end;
    }

    // General mbed initialization
    mbedtls_ssl_config_init(&network_tls_config->config);
    mbedtls_entropy_init(&network_tls_config->entropy);
    mbedtls_ctr_drbg_init(&network_tls_config->ctr_drbg);

    // TODO: this allows only 1 certificate per configuration
    mbedtls_x509_crt_init(&network_tls_config->srvcert);
    mbedtls_pk_init(&network_tls_config->srvkey);

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
            &network_tls_config->srvcert,
            certificate_path)) {
        goto end;
    }

    if (!network_tls_load_private_key(
            &network_tls_config->srvkey,
            private_key_path)) {
        goto end;
    }

    mbedtls_ssl_conf_ca_chain(
            &network_tls_config->config,
            network_tls_config->srvcert.next,
            NULL);

    if (mbedtls_ssl_conf_own_cert(
            &network_tls_config->config,
            &network_tls_config->srvcert,
            &network_tls_config->srvkey) != 0) {
        goto end;
    }

    mbedtls_ssl_conf_ciphersuites(
            &network_tls_config->config,
            chiper_suites);

    // Disable renegotiation & legacy renegotiation as in general regeneration is not supported by kTLS, it has been
    // removed from TLS 1.3 anyway
    mbedtls_ssl_conf_renegotiation(
            &network_tls_config->config,
            MBEDTLS_SSL_RENEGOTIATION_DISABLED);
    mbedtls_ssl_conf_legacy_renegotiation(
            &network_tls_config->config,
            MBEDTLS_SSL_LEGACY_NO_RENEGOTIATION);

    // Enable TLS 1.2 and newer
    mbedtls_ssl_conf_min_version(
            &network_tls_config->config,
            MBEDTLS_SSL_MAJOR_VERSION_3,
            MBEDTLS_SSL_MINOR_VERSION_3);

    return_res = true;

end:
    if (!return_res && network_tls_config) {
        mbedtls_pk_free(&network_tls_config->srvkey);
        mbedtls_x509_crt_free(&network_tls_config->srvcert);
        mbedtls_ctr_drbg_free(&network_tls_config->ctr_drbg);
        mbedtls_entropy_free(&network_tls_config->entropy);
        mbedtls_ssl_config_free(&network_tls_config->config);

        slab_allocator_mem_free(network_tls_config);
        network_tls_config = NULL;
    }

    return network_tls_config;
}

void network_tls_config_free(
        network_tls_config_t *network_tls_config) {
    if (network_tls_config == NULL) {
        return;
    }

    mbedtls_ctr_drbg_free(&network_tls_config->ctr_drbg);
    mbedtls_entropy_free(&network_tls_config->entropy);
    mbedtls_ssl_config_free(&network_tls_config->config);

    slab_allocator_mem_free(network_tls_config);
}
