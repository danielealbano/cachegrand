#ifndef CACHEGRAND_NETWORK_CHANNEL_IOURING_H
#define CACHEGRAND_NETWORK_CHANNEL_IOURING_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct network_channel_iouring network_channel_iouring_t;
struct network_channel_iouring {
    network_channel_t wrapped_channel;
    network_io_common_fd_t mapped_fd;
    bool has_mapped_fd;
    int base_sqe_flags;
    network_io_common_fd_t  fd;
} __attribute__((__aligned__(32)));

network_channel_iouring_t* network_channel_iouring_new();

network_channel_iouring_t* network_channel_iouring_multi_new(
        uint32_t count);

void network_channel_iouring_free(
        network_channel_iouring_t* network_channel);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_CHANNEL_IOURING_H
