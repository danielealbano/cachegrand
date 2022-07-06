#ifndef CACHEGRAND_PROGRAM_ULIMIT_H
#define CACHEGRAND_PROGRAM_ULIMIT_H

#ifdef __cplusplus
extern "C" {
#endif

#define PROGRAM_ULIMIT_NOFILE 0x80000
#define PROGRAM_ULIMIT_MEMLOCK 0xFFFFFFFFUL

void program_ulimit_setup();

bool program_ulimit_wrapper(
        __rlimit_resource_t resource,
        ulong value);

bool program_ulimit_set_nofile(
        ulong value);

bool program_ulimit_set_memlock(
        ulong value);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_PROGRAM_ULIMIT_H
