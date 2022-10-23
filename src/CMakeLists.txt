# Copyright (C) 2018-2022 Daniele Salvatore Albano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.

add_subdirectory(module)

macro(CACHEGRAND_BUILD_ADHOC_HASHTABLE_MCMP_SUPPORT_OP_ARCH ARCH)
    if(ARCH_IS_X86_64)
        set(HASHTABLE_MCMP_SUPPORT_OP_ARCH_OPT_avx512f -mno-avx256-split-unaligned-load -mavx512f -mavx2 -mavx -mbmi -mbmi2 -mfma -mtune=skylake)
        set(HASHTABLE_MCMP_SUPPORT_OP_ARCH_OPT_avx2 -mno-avx512f -mno-avx256-split-unaligned-load -mavx2 -mavx -mavx -mbmi -mbmi2 -mfma -mtune=haswell)
        set(HASHTABLE_MCMP_SUPPORT_OP_ARCH_OPT_avx -mno-avx512f -mno-avx2 -mno-avx256-split-unaligned-load -mavx -mbmi -mbmi2)
        set(HASHTABLE_MCMP_SUPPORT_OP_ARCH_OPT_loop -mno-avx512f -mno-avx2 -mno-avx)
    elseif(ARCH_IS_AARCH64)
        set(HASHTABLE_MCMP_SUPPORT_OP_ARCH_OPT_armv8a_neon -march=armv8-a)
        set(HASHTABLE_MCMP_SUPPORT_OP_ARCH_OPT_loop "")
    endif()

    message(STATUS "Enabling accelerated hashtable support operations -- ${ARCH}")

    set(hashtable_mcmp_support_op_arch_target_name "cachegrand_internal_hashtable_mcmp_support_op_arch_${ARCH}")

    add_library(
            ${hashtable_mcmp_support_op_arch_target_name}
            OBJECT
            "${CMAKE_CURRENT_SOURCE_DIR}/data_structures/hashtable/mcmp/hashtable_support_op_arch.c")

    set_target_properties(
            ${hashtable_mcmp_support_op_arch_target_name}
            PROPERTIES
            LINKER_LANGUAGE C)

    target_compile_definitions(
            ${hashtable_mcmp_support_op_arch_target_name}
            PRIVATE
            CACHEGRAND_HASHTABLE_MCMP_SUPPORT_OP_ARCH_SUFFIX=${ARCH})

    target_compile_options(
            "${hashtable_mcmp_support_op_arch_target_name}"
            PRIVATE
            ${HASHTABLE_MCMP_SUPPORT_OP_ARCH_OPT_${ARCH}})

    target_include_directories(
            ${hashtable_mcmp_support_op_arch_target_name}
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR} ${DEPS_LIST_INCLUDE_DIRS} "${CMAKE_CURRENT_SOURCE_DIR}/data_structures/hashtable/mcmp")

    list(APPEND SRC_FILES_CACHEGRAND "$<TARGET_OBJECTS:${hashtable_mcmp_support_op_arch_target_name}>")
endmacro()

# Check the selected hash algorithm
set(HASH_ALGORITHM_SELECTED_COUNTER 0)
if (USE_HASH_ALGORITHM_XXH3)
    math(EXPR HASH_ALGORITHM_SELECTED_COUNTER "${HASH_ALGORITHM_SELECTED_COUNTER}+1")
    message(STATUS "Hashtable hashing algorithm - xxh3")
endif()

if (USE_HASH_ALGORITHM_T1HA2)
    math(EXPR HASH_ALGORITHM_SELECTED_COUNTER "${HASH_ALGORITHM_SELECTED_COUNTER}+1")
    message(STATUS "Hashtable hashing algorithm - t1ha2")
endif()

if (USE_HASH_ALGORITHM_CRC32C)
    math(EXPR HASH_ALGORITHM_SELECTED_COUNTER "${HASH_ALGORITHM_SELECTED_COUNTER}+1")
    message(STATUS "Hashtable hashing algorithm - crc32c")
endif()

if(HASH_ALGORITHM_SELECTED_COUNTER GREATER 1)
    message(FATAL_ERROR "Can't enable multiple hashing algorithm for the hashtable, select one between xxh3, t1ha2 and crc32c")
elseif(HASH_ALGORITHM_SELECTED_COUNTER EQUAL 0)
    message(FATAL_ERROR "Please select one hashing algorithm for the hashtable")
endif()

# Build the base cachegrand library
file(GLOB_RECURSE SRC_FILES_CACHEGRAND "*.c")
list(APPEND SRC_FILES_CACHEGRAND "${CACHEGRAND_CMAKE_CONFIG_C_SRC}")
list(REMOVE_ITEM SRC_FILES_CACHEGRAND ${CMAKE_CURRENT_SOURCE_DIR}/main.c)

# Remove all the architecure dependant impmentation of the string functions -- avx2
list(REMOVE_ITEM SRC_FILES_CACHEGRAND "${CMAKE_CURRENT_SOURCE_DIR}/utils_string_avx2.c")

# Remove all the architecture dependant implementation of the hash crc32 algorithm
list(REMOVE_ITEM SRC_FILES_CACHEGRAND "${CMAKE_CURRENT_SOURCE_DIR}/hash/hash_crc32c_sse42.c")

# Remove all the architecture dependant implementation of the search loop and re-include it
list(FILTER SRC_FILES_CACHEGRAND EXCLUDE REGEX ".*hashtable_support_op_arch.c$")

if(ARCH_IS_X86_64)
    # Fiber context
    message(STATUS "Enabling Fiber context arch x86-64")
    list(APPEND SRC_FILES_CACHEGRAND "${CMAKE_CURRENT_SOURCE_DIR}/fiber/arch/x86-64/fiber_context.s")

    message(STATUS "Enabling accelerated string functions")

    # utils_string_avx2.c
    message(STATUS "Enabling accelerated string functions -- avx2")
    list(APPEND SRC_FILES_CACHEGRAND "${CMAKE_CURRENT_SOURCE_DIR}/utils_string_avx2.c")
    set_source_files_properties(
            "utils_string_avx2.c"
            PROPERTIES COMPILE_FLAGS
            "-mno-avx256-split-unaligned-load -mavx2 -mbmi -mfma -mtune=haswell")

    message(STATUS "Enabling accelerated crc32c hash")

    # hash/hash_crc32c_sse42.c
    message(STATUS "Enabling accelerated crc32c hash -- sse4.2")
    list(APPEND SRC_FILES_CACHEGRAND "${CMAKE_CURRENT_SOURCE_DIR}/hash/hash_crc32c_sse42.c")
    set_source_files_properties(
            "hash/hash_crc32c_sse42.c"
            PROPERTIES COMPILE_FLAGS
            "-msse4.2 -mpclmul")

    message(STATUS "Enabling accelerated hashtable support operations")

    if (ENABLE_SUPPORT_AVX512F)
        CACHEGRAND_BUILD_ADHOC_HASHTABLE_MCMP_SUPPORT_OP_ARCH("avx512f")
    endif()

    foreach(HASHTABLE_MCMP_SUPPORT_OP_ARCH_SUFFIX
            avx2
            avx
            loop)
        CACHEGRAND_BUILD_ADHOC_HASHTABLE_MCMP_SUPPORT_OP_ARCH(
                ${HASHTABLE_MCMP_SUPPORT_OP_ARCH_SUFFIX})
    endforeach()

elseif(ARCH_IS_AARCH64)
    message(STATUS "No accelerated implementations for AARCH64, using portable")

    # Fiber context
    message(STATUS "Enabling Fiber context arch aarch64")
    list(APPEND SRC_FILES_CACHEGRAND "${CMAKE_CURRENT_SOURCE_DIR}/fiber/arch/aarch64/fiber_context.s")

    foreach(HASHTABLE_MCMP_SUPPORT_OP_ARCH_SUFFIX
            armv8a_neon
            loop)
        CACHEGRAND_BUILD_ADHOC_HASHTABLE_MCMP_SUPPORT_OP_ARCH(
                ${HASHTABLE_MCMP_SUPPORT_OP_ARCH_SUFFIX})
    endforeach()
else()
    message(FATAL_ERROR "Unsupported architecture")
endif()

add_library(
        cachegrand-internal
        ${SRC_FILES_CACHEGRAND})

set_target_properties(
        cachegrand-internal
        PROPERTIES
        LINKER_LANGUAGE C)

set_target_properties(
        cachegrand-internal
        PROPERTIES
        ENABLE_EXPORTS ON
)

add_dependencies(
        cachegrand-internal
        __internal_refresh_cmake_config uring cyaml
        cachegrand-internal-module-redis-autogenerated-commands)

target_compile_options(
        cachegrand-internal
        PUBLIC
        -include cmake_config.h)

if (FFMA_DEBUG_ALLOCS_FREES)
    target_compile_options(
            cachegrand-internal
            PUBLIC
            -DFFMA_DEBUG_ALLOCS_FREES=1)
endif()

target_link_directories(
        cachegrand-internal
        PUBLIC
        ${DEPS_LIST_LIBRARY_DIRS})

target_link_libraries(
        cachegrand-internal
        PUBLIC
        ${DEPS_LIST_LIBRARIES} -latomic)
target_link_libraries(
        cachegrand-internal
        PRIVATE
        ${DEPS_LIST_LIBRARIES_PRIVATE})

target_include_directories(
        cachegrand-internal
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}" ${DEPS_LIST_INCLUDE_DIRS})

if(ARCH_IS_X86_64)
    # Force GCC to emit cmpxchg16b inline (supported only on x86_64 platforms)
    target_compile_options(
            cachegrand-internal
            PUBLIC
            -mcx16)
endif()

if (CMAKE_C_LINK_PIE_SUPPORTED AND CMAKE_BUILD_TYPE MATCHES Release)
    set_property(TARGET cachegrand-internal PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()

# Build the executable
add_executable(
        cachegrand-server
        "main.c")

set_target_properties(
        cachegrand-server
        PROPERTIES
        LINKER_LANGUAGE C)

add_dependencies(
        cachegrand-server
        cachegrand-internal)

target_link_libraries(
        cachegrand-server
        cachegrand-internal ${DEPS_LIST_LIBRARIES_PRIVATE})

if (CMAKE_C_LINK_PIE_SUPPORTED AND CMAKE_BUILD_TYPE MATCHES Release)
    set_property(TARGET cachegrand-server PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()
