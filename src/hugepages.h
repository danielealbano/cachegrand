#ifndef CACHEGRAND_HUGEPAGES_H
#define CACHEGRAND_HUGEPAGES_H

#ifdef __cplusplus
extern "C" {
#endif

#define HUGEPAGES_SYSFS_PATH "/sys/kernel/mm/hugepages/"
#define HUGEPAGES_SYSFS_2MB_PATH HUGEPAGES_SYSFS_PATH "hugepages-2048kB/"
#define HUGEPAGES_SYSFS_1024MB_PATH HUGEPAGES_SYSFS_PATH "hugepages-1048576kB/"

#define HUGEPAGES_SYSFS_NR_HUGEPAGES_FILENAME "nr_hugepages"
#define HUGEPAGES_SYSFS_RESV_HUGEPAGES_FILENAME "resv_hugepages"
#define HUGEPAGES_SYSFS_FREE_HUGEPAGES_FILENAME "free_hugepages"

bool hugepages_file_read(
        const char* path,
        char* out_data,
        size_t out_data_len);

bool hugepages_file_read_uint32(
        const char* path,
        uint32_t* out_data);

uint32_t hugepages_file_path_uint32_return(
        const char* path);

bool hugepages_2mb_is_available(
        int min_available);

bool hugepages_1024mb_is_available(
        int min_available);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HUGEPAGES_H
