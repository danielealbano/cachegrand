#ifndef CACHEGRAND_IO_URING_SUPPORTED_H
#define CACHEGRAND_IO_URING_SUPPORTED_H

#ifdef __cplusplus
extern "C" {
#endif

bool io_uring_supported_fetch_kallsyms_symbol_name(
        FILE* fd,
        char* buffer,
        size_t buffer_size);
bool io_uring_supported_is_expected_symbol_name(
        char* buffer,
        size_t buffer_size);
FILE* io_uring_supported_open_kallsyms();
bool io_uring_supported();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_IO_URING_SUPPORTED_H
