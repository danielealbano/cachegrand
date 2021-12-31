#ifndef CACHEGRAND_HUGEPAGES_H
#define CACHEGRAND_HUGEPAGES_H

#ifdef __cplusplus
extern "C" {
#endif

#define HUGEPAGE_SIZE_2MB   (2 * 1024 * 1024)

#define HUGEPAGES_SYSFS_PATH "/sys/kernel/mm/hugepages/"
#define HUGEPAGES_SYSFS_2MB_PATH HUGEPAGES_SYSFS_PATH "hugepages-2048kB/"
#define HUGEPAGES_SYSFS_1024MB_PATH HUGEPAGES_SYSFS_PATH "hugepages-1048576kB/"

#define HUGEPAGES_SYSFS_NR_HUGEPAGES_FILENAME "nr_hugepages"
#define HUGEPAGES_SYSFS_RESV_HUGEPAGES_FILENAME "resv_hugepages"
#define HUGEPAGES_SYSFS_FREE_HUGEPAGES_FILENAME "free_hugepages"

bool hugepages_2mb_is_available(
        int min_available);

bool hugepages_1024mb_is_available(
        int min_available);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HUGEPAGES_H
