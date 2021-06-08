#ifndef CACHEGRAND_NETWORK_CHANNEL_IOURING_H
#define CACHEGRAND_NETWORK_CHANNEL_IOURING_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct network_channel_iouring network_channel_iouring_t;
struct network_channel_iouring {
    network_channel_t wrapped_channel;
    int mapped_fd;
    bool has_mapped_fd;
    int base_sqe_flags;
    int fd;
};

network_channel_iouring_t* network_channel_iouring_new();

network_channel_iouring_t* network_channel_iouring_new_multi(
        int count);

void network_channel_iouring_free(
        network_channel_iouring_t* network_channel);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_CHANNEL_IOURING_H
