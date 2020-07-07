#ifndef CACHEGRAND_VERSION_H
#define CACHEGRAND_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

int version_parse(
        char *release_string,
        long *version,
        size_t version_size);
int version_compare(
        const long version1[],
        const long version2[],
        uint8_t version_parts_count);
bool version_kernel(
        long *kernel_version,
        int parts_count);
bool version_kernel_min(
        long min_kernel_version[4],
        int parts_count);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_VERSION_H
