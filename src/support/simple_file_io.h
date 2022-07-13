#ifndef CACHEGRAND_SIMPLE_FILE_IO_H
#define CACHEGRAND_SIMPLE_FILE_IO_H

#ifdef __cplusplus
extern "C" {
#endif

bool simple_file_io_exists(
        const char* path);
bool simple_file_io_read(
        const char *path,
        char *out_data,
        size_t out_data_len);

bool simple_file_io_read_uint32(
        const char *path,
        uint32_t *out_data);

uint32_t simple_file_io_read_uint32_return(
        const char *path);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_SIMPLE_FILE_IO_H
