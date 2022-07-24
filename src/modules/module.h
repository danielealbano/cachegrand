#ifndef CACHEGRAND_MODULE_H
#define CACHEGRAND_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

enum network_protocols {
    NETWORK_PROTOCOLS_UNKNOWN = 0,
    NETWORK_PROTOCOLS_REDIS,
    NETWORK_PROTOCOLS_PROMETHEUS
};
typedef enum network_protocols network_protocols_t;

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_H
