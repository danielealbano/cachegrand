#ifndef CACHEGRAND_EXTTYPES_H
#define CACHEGRAND_EXTTYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __kernel_timespec kernel_timespec_t;

// Defines the int128_t and uint128_t
typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;

// Defines the wrapper for the volatile type and creates a set of types based off the standard uint* using it
#define _Volatile(T)    volatile T

typedef _Volatile(uint8_t) uint8_volatile_t;
typedef _Volatile(uint16_t) uint16_volatile_t;
typedef _Volatile(uint32_t) uint32_volatile_t;
typedef _Volatile(uint64_t) uint64_volatile_t;
typedef _Volatile(uint128_t) uint128_volatile_t;
typedef _Volatile(int8_t) int8_volatile_t;
typedef _Volatile(int16_t) int16_volatile_t;
typedef _Volatile(int32_t) int32_volatile_t;
typedef _Volatile(int64_t) int64_volatile_t;
typedef _Volatile(int128_t) int128_volatile_t;
typedef _Volatile(bool) bool_volatile_t;

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_EXTTYPES_H
