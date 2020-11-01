#ifndef CACHEGRAND_NETWORK_PROTOCOL_H
#define CACHEGRAND_NETWORK_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

enum network_protocol_type {
    NETWORK_PROTOCOL_TYPE_UNKNOWN = 0,
    NETWORK_PROTOCOL_TYPE_REDIS,
};
typedef enum network_protocol_type network_protocol_type_t;

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_PROTOCOL_H
