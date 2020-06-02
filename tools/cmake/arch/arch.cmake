include(get-target-arch)

message(STATUS "Fetching architecture")
get_target_arch(TARGET_ARCH)
message(STATUS "Fetching architecture -- ${TARGET_ARCH}")

include("arch-${TARGET_ARCH}")
