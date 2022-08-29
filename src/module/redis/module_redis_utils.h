#ifndef CACHEGRAND_MODULE_REDIS_UTILS_H
#define CACHEGRAND_MODULE_REDIS_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

bool module_redis_glob_match(
        char *string,
        size_t string_length,
        char *pattern,
        size_t pattern_length);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_REDIS_UTILS_H
