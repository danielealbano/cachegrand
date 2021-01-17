include(get-target-arch)

message(STATUS "Fetching architecture")
get_target_arch(CACHEGRAND_ARCH_TARGET)
message(STATUS "Fetching architecture -- ${CACHEGRAND_ARCH_TARGET}")

set(HOST_HAS_AVX2 0)
set(HOST_HAS_AVX 0)
set(HOST_HAS_CLFLUSHOPT 0)
set(HOST_HAS_SSE42 0)

include("arch-${CACHEGRAND_ARCH_TARGET}")
