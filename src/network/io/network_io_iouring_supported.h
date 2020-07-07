#ifndef CACHEGRAND_NETWORK_IO_IOURING_SUPPORTED_H
#define CACHEGRAND_NETWORK_IO_IOURING_SUPPORTED_H

#ifdef __cplusplus
extern "C" {
#endif

bool network_io_iouring_supported_fetch_kallsyms_symbol_name(
        FILE* fd,
        char* buffer,
        size_t buffer_size);
bool network_io_iouring_supported_is_expected_symbol_name(
        char* buffer,
        size_t buffer_size);
FILE* network_io_iouring_supported_open_kallsyms();
bool network_io_iouring_supported();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_IO_IOURING_SUPPORTED_H
