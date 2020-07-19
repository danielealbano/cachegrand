#ifndef CACHEGRAND_IO_URING_CAPABILITIES_H
#define CACHEGRAND_IO_URING_CAPABILITIES_H

#ifdef __cplusplus
extern "C" {
#endif

bool io_uring_capabilities_kallsyms_fetch_symbol_name(
        FILE* fd,
        char* buffer,
        size_t buffer_size);
bool io_uring_capabilities_kallsyms_is_expected_symbol_name(
        char* buffer,
        size_t buffer_size);
FILE* io_uring_capabilities_kallsyms_open();
bool io_uring_capabilities_kallsyms_ensure_iouring_available();
bool io_uring_capabilities_is_linked_op_files_update_supported();
bool io_uring_capabilities_is_supported();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_IO_URING_CAPABILITIES_H
