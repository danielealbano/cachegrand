#ifndef CACHEGRAND_STORAGE_CHANNEL_H
#define CACHEGRAND_STORAGE_CHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct storage_channel storage_channel_t;
struct storage_channel {
    storage_io_common_fd_t fd;
    char *path;
    size_t path_len;
};

bool storage_channel_init(
        storage_channel_t *channel);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_STORAGE_CHANNEL_H
