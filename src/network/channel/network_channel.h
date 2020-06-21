#ifndef CACHEGRAND_NETWORK_CHANNEL_H
#define CACHEGRAND_NETWORK_CHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct network_channel_config network_channel_config_t;
struct network_channel_config {
    in_port_t port;
    in_addr_t addr;
};

typedef struct network_channel network_channel_t;
struct network_channel {
    network_channel_config_t* config;
    union {
        network_channel_iouring_t iouring;
    };
};

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_CHANNEL_H
