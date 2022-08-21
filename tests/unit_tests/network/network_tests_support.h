#ifndef CACHEGRAND_NETWORK_TESTS_SUPPORT_H
#define CACHEGRAND_NETWORK_TESTS_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

uint16_t network_tests_support_search_free_port_ipv4(
        uint16_t start_port);
uint16_t network_tests_support_search_free_port_ipv6(
        uint16_t start_port);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_TESTS_SUPPORT_H
