#ifndef CACHEGRAND_WORKER_STORAGE_IOURING_OP_H
#define CACHEGRAND_WORKER_STORAGE_IOURING_OP_H

#ifdef __cplusplus
extern "C" {
#endif

bool worker_storage_iouring_complete_op_simple();

storage_channel_t* worker_storage_iouring_op_storage_open(
        char *path,
        storage_io_common_open_flags_t flags,
        storage_io_common_open_mode_t mode);

size_t worker_storage_iouring_op_storage_read(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        off_t offset);

size_t worker_storage_iouring_op_storage_write(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        off_t offset);

bool worker_storage_iouring_op_storage_flush(
        storage_channel_t *channel);

bool worker_storage_iouring_op_storage_fallocate(
        storage_channel_t *channel,
        int mode,
        off_t offset,
        off_t len);

bool worker_storage_iouring_op_storage_close(
        storage_channel_t *channel);

bool worker_storage_iouring_initialize(
        worker_context_t *worker_context);

void worker_storage_iouring_cleanup(
        worker_context_t *worker_context);

void worker_storage_iouring_op_register();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_STORAGE_IOURING_OP_H
