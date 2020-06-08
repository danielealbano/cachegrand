include(get-target-arch)

message(STATUS "Fetching architecture")
get_target_arch(CACHEGRAND_ARCH_TARGET)
message(STATUS "Fetching architecture -- ${CACHEGRAND_ARCH_TARGET}")

include("arch-${CACHEGRAND_ARCH_TARGET}")
