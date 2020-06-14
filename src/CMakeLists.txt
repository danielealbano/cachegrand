macro(CACHEGRAND_BUILD_ADHOC_HASHTABLE_SUPPORT_OP_ARCH ARCH)
    set(HASHTABLE_SUPPORT_OP_ARCH_OPT_avx2 -mtune=haswell)
    set(HASHTABLE_SUPPORT_OP_ARCH_OPT_avx -mno-avx256-split-unaligned-load -mno-avx2 -mavx -mbmi)
    set(HASHTABLE_SUPPORT_OP_ARCH_OPT_sse42 -msse4.2)
    set(HASHTABLE_SUPPORT_OP_ARCH_OPT_genericx64 -mtune=k8)
    set(HASHTABLE_SUPPORT_OP_ARCH_OPT_noopt "")

    if(NOT ${ARCH} STREQUAL "noopt")
        message(STATUS "Enabling accelerated hashtable support operations -- ${ARCH}")
    endif()

    set(hashtable_support_op_arch_target_name "cachegrand_internal_hashtable_support_op_arch_${ARCH}")

    add_library(
            ${hashtable_support_op_arch_target_name}
            OBJECT
            "${CMAKE_CURRENT_SOURCE_DIR}/hashtable/hashtable_support_op_arch.c")
    target_compile_definitions(
            "${hashtable_support_op_arch_target_name}"
            PRIVATE
            "-DCACHEGRAND_HASHTABLE_SUPPORT_OP_ARCH_SUFFIX=${ARCH}")
    target_compile_options(
            "${hashtable_support_op_arch_target_name}"
            PRIVATE
            ${HASHTABLE_SUPPORT_OP_ARCH_OPT_${ARCH}})
    target_include_directories(
            ${hashtable_support_op_arch_target_name}
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR})

    add_dependencies(cachegrand-internal ${hashtable_support_op_arch_target_name})
    list(APPEND SRC_FILES_CACHEGRAND_HASHTABLE "$<TARGET_OBJECTS:${hashtable_support_op_arch_target_name}>")
endmacro()

# Build the base cachegrand library
file(GLOB SRC_FILES_CACHEGRAND "*.c")
file(GLOB SRC_FILES_CACHEGRAND_HASHTABLE "hashtable/*.c")
list(APPEND SRC_FILE_CACHEGRAND "${CACHEGRAND_CMAKE_CONFIG_C_SRC}")
list(REMOVE_ITEM SRC_FILES_CACHEGRAND ${CMAKE_CURRENT_SOURCE_DIR}/main.c)

# Remove all the architecture dependant implementation but reinclude the loop (non architecture dependant)
list(FILTER SRC_FILES_CACHEGRAND_HASHTABLE EXCLUDE REGEX ".*hashtable_support_hash_search_[a-z0-9]+\\.c$")
list(FILTER SRC_FILES_CACHEGRAND_HASHTABLE EXCLUDE REGEX ".*hashtable_support_op_arch\\.c$")
list(APPEND SRC_FILES_CACHEGRAND_HASHTABLE "${CMAKE_CURRENT_SOURCE_DIR}/hashtable/hashtable_support_hash_search_loop.c")

add_library(
        cachegrand-internal
        ${SRC_FILES_CACHEGRAND}
        ${SRC_FILES_CACHEGRAND_HASHTABLE}
)
add_dependencies(cachegrand-internal
        __internal_refresh_cmake_config_c
        t1ha)
target_link_libraries(cachegrand-internal t1ha)
target_include_directories(cachegrand-internal PUBLIC "../3rdparty/t1ha/")
target_include_directories(cachegrand-internal PUBLIC ".")

# Build the ad-hoc versions of hashtable_support_op.c
CACHEGRAND_BUILD_ADHOC_HASHTABLE_SUPPORT_OP_ARCH(
        noopt
        SRC_FILES_CACHEGRAND_HASHTABLE)

if(ARCH_IS_X86_64)
    # Force GCC to emit cmpxchg16b inline (supported only on x86_64 platforms)
    target_compile_options(cachegrand-internal PUBLIC -mcx16)

    message(STATUS "Enabling accelerated hash search -- avx2")
    list(APPEND SRC_FILES_CACHEGRAND_HASHTABLE "${CMAKE_CURRENT_SOURCE_DIR}/hashtable/hashtable_support_hash_search_avx2.c")
    set_source_files_properties(
            "hashtable/hashtable_support_hash_search_avx2.c"
            PROPERTIES COMPILE_FLAGS
            "-mno-avx256-split-unaligned-load -mavx2 -mbmi")

    message(STATUS "Enabling accelerated hash search -- avx")
    list(APPEND SRC_FILES_CACHEGRAND_HASHTABLE "${CMAKE_CURRENT_SOURCE_DIR}/hashtable/hashtable_support_hash_search_avx.c")
    set_source_files_properties(
            "hashtable/hashtable_support_hash_search_avx.c"
            PROPERTIES COMPILE_FLAGS
            "-mno-avx256-split-unaligned-load -mno-avx2 -mavx -mbmi")

    foreach(HASHTABLE_SUPPORT_OP_ARCH_SUFFIX
            avx2
            avx
            sse42
            genericx64)
        CACHEGRAND_BUILD_ADHOC_HASHTABLE_SUPPORT_OP_ARCH(
                ${HASHTABLE_SUPPORT_OP_ARCH_SUFFIX}
                SRC_FILES_CACHEGRAND_HASHTABLE)
    endforeach()
else()
    message(FATAL_ERROR "Unsupported architecture")
endif()

IF(WIN32)
    target_link_libraries(cachegrand-internal imagehlp)
ENDIF(WIN32)

# Build the executable
add_executable(cachegrand "main.c")
add_dependencies(cachegrand cachegrand-internal t1ha)
target_link_libraries(cachegrand cachegrand-internal t1ha)
target_include_directories(cachegrand PUBLIC ".")
