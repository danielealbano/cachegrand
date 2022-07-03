# Copyright (C) 2018-2022 Daniele Salvatore Albano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.

include(get-target-arch)

message(STATUS "Fetching architecture")
get_target_arch(CACHEGRAND_ARCH_TARGET)
message(STATUS "Fetching architecture -- ${CACHEGRAND_ARCH_TARGET}")

include("arch-${CACHEGRAND_ARCH_TARGET}")

if (NOT HOST_HAS_AVX512F)
    set(HOST_HAS_AVX512F 0)
endif()
if (NOT HOST_HAS_AVX2)
    set(HOST_HAS_AVX2 0)
endif()
if (NOT HOST_HAS_AVX)
    set(HOST_HAS_AVX 0)
endif()
if (NOT HOST_HAS_CLFLUSHOPT)
    set(HOST_HAS_CLFLUSHOPT 0)
endif()
if (NOT HOST_HAS_SSE42)
    set(HOST_HAS_SSE42 0)
endif()
