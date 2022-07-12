#ifndef CACHEGRAND_NETWORK_TLS_H
#define CACHEGRAND_NETWORK_TLS_H

#ifdef __cplusplus
extern "C" {
#endif

#define NETWORK_TLS_PROC_SYS_NET_IPV4_TCP_AVAILABLE_ULP "/proc/sys/net/ipv4/tcp_available_ulp"

// The struct is defined in network_tls_internal.h
typedef struct network_tls_config network_tls_config_t;

typedef struct network_tls_mbedtls_cipher_suite_info network_tls_mbedtls_cipher_suite_info_t;
struct network_tls_mbedtls_cipher_suite_info
{
    const char *name;
    bool offloading;
    config_network_protocol_tls_min_version_t min_version;
    config_network_protocol_tls_max_version_t max_version;
};

bool network_tls_is_ulp_tls_supported_internal();

int *network_tls_build_cipher_suites_from_names(
        char **cipher_suite_name,
        unsigned int cipher_suites_names_count,
        size_t *cipher_suites_ids_size);

network_tls_config_t *network_tls_config_init(
        char *certificate_path,
        char *private_key_path,
        config_network_protocol_tls_min_version_t tls_min_version,
        config_network_protocol_tls_max_version_t tls_max_version,
        int *cipher_suites,
        size_t cipher_suites_length);

void network_tls_config_free(
        network_tls_config_t *network_tls_config);

char* network_tls_mbedtls_version();

network_tls_mbedtls_cipher_suite_info_t *network_tls_mbedtls_get_all_cipher_suites_info();

char *network_tls_min_version_to_string(
        config_network_protocol_tls_min_version_t version);

char *network_tls_max_version_to_string(
        config_network_protocol_tls_max_version_t version);

static bool tls_ulp_supported_fetched = false;
static bool tls_ulp_supported = false;
static inline bool network_tls_is_ulp_tls_supported() {
    if (tls_ulp_supported_fetched == false) {
        tls_ulp_supported_fetched = true;
        tls_ulp_supported = network_tls_is_ulp_tls_supported_internal();
    }

    return tls_ulp_supported;
}

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_TLS_H
