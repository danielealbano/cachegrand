#ifndef CACHEGRAND_WORKER_STORAGE_H
#define CACHEGRAND_WORKER_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

storage_channel_t* worker_storage_open(
        char *path,
        storage_io_common_open_flags_t flags,
        storage_io_common_open_mode_t mode);

bool worker_storage_readv(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        size_t expected_read_len,
        off_t offset);

bool worker_storage_read(
        storage_channel_t *channel,
        char *buffer,
        size_t buffer_len,
        off_t offset);

bool worker_storage_writev(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        size_t expected_write_len,
        off_t offset);

bool worker_storage_write(
        storage_channel_t *channel,
        char *buffer,
        size_t buffer_len,
        off_t offset);

bool worker_storage_flush(
        storage_channel_t *channel);
bool worker_storage_fallocate(
        storage_channel_t *channel,
        int mode,
        off_t offset,
        off_t len);

bool worker_storage_close(
        storage_channel_t *channel);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_STORAGE_H
