#ifndef CACHEGRAND_STORAGE_H
#define CACHEGRAND_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

storage_channel_t* storage_open(
        char *path,
        storage_io_common_open_flags_t flags,
        storage_io_common_open_mode_t mode);

bool storage_readv(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        size_t expected_read_len,
        off_t offset);

bool storage_read(
        storage_channel_t *channel,
        char *buffer,
        size_t buffer_len,
        off_t offset);

bool storage_writev(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        size_t expected_write_len,
        off_t offset);

bool storage_write(
        storage_channel_t *channel,
        char *buffer,
        size_t buffer_len,
        off_t offset);

bool storage_flush(
        storage_channel_t *channel);

bool storage_fallocate(
        storage_channel_t *channel,
        int mode,
        off_t offset,
        off_t len);

bool storage_close(
        storage_channel_t *channel);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_STORAGE_H
