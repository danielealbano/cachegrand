#ifndef CACHEGRAND_STORAGE_H
#define CACHEGRAND_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int storage_io_common_fd_t;

typedef int storage_io_common_open_flags_t;
typedef mode_t storage_io_common_open_mode_t;
typedef struct iovec storage_io_common_iovec_t;

bool storage_io_common_close(
        storage_io_common_fd_t fd);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_STORAGE_H
