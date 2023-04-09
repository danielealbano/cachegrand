#ifndef CACHEGRAND_WORKER_STORAGE_OP_H
#define CACHEGRAND_WORKER_STORAGE_OP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef storage_channel_t *(worker_op_storage_open_fp_t)(
        char *path,
        storage_io_common_open_flags_t flags,
        storage_io_common_open_mode_t mode);

typedef storage_channel_t *(worker_op_storage_open_fd_fp_t)(
        storage_io_common_fd_t fd);

typedef int32_t (worker_op_storage_read_fp_t)(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        off_t offset);

typedef int32_t (worker_op_storage_write_fp_t)(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        off_t offset);

typedef bool (worker_op_storage_flush_fp_t)(
        storage_channel_t *channel);

typedef bool (worker_op_storage_fallocate_fp_t)(
        storage_channel_t *channel,
        int mode,
        off_t offset,
        off_t len);

typedef bool (worker_op_storage_close_fp_t)(
        storage_channel_t *channel);

// Storage operations
extern worker_op_storage_open_fp_t *worker_op_storage_open;
extern worker_op_storage_open_fd_fp_t *worker_op_storage_open_fd;
extern worker_op_storage_read_fp_t *worker_op_storage_read;
extern worker_op_storage_write_fp_t *worker_op_storage_write;
extern worker_op_storage_flush_fp_t *worker_op_storage_flush;
extern worker_op_storage_fallocate_fp_t *worker_op_storage_fallocate;
extern worker_op_storage_close_fp_t *worker_op_storage_close;

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_STORAGE_OP_H
