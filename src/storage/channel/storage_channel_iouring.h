#ifndef CACHEGRAND_STORAGE_CHANNEL_IOURING_H
#define CACHEGRAND_STORAGE_CHANNEL_IOURING_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct storage_channel_iouring storage_channel_iouring_t;
struct storage_channel_iouring {
    storage_channel_t wrapped_channel;
    storage_io_common_fd_t mapped_fd;
    bool has_mapped_fd;
    int base_sqe_flags;
    storage_io_common_fd_t fd;
} __attribute__((__aligned__(32)));

storage_channel_iouring_t* storage_channel_iouring_new();

void storage_channel_iouring_free(
        storage_channel_iouring_t* storage_channel);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_STORAGE_CHANNEL_IOURING_H
