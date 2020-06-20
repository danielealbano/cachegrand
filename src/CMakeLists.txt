macro(CACHEGRAND_BUILD_ADHOC_HASHTABLE_SUPPORT_OP_ARCH ARCH)
    set(HASHTABLE_SUPPORT_OP_ARCH_OPT_avx2 -mavx2 -mbmi -mfma -mtune=haswell)
    set(HASHTABLE_SUPPORT_OP_ARCH_OPT_avx -mno-avx256-split-unaligned-load -mno-avx2 -mavx -mbmi)
    set(HASHTABLE_SUPPORT_OP_ARCH_OPT_sse42 -msse4.2)
    set(HASHTABLE_SUPPORT_OP_ARCH_OPT_ss3 -msse3)

    message(STATUS "Enabling accelerated hashtable support operations -- ${ARCH}")

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

#    add_dependencies(cachegrand-internal ${hashtable_support_op_arch_target_name})
    list(APPEND SRC_FILES_CACHEGRAND "$<TARGET_OBJECTS:${hashtable_support_op_arch_target_name}>")
endmacro()

# Build the base cachegrand library
file(GLOB SRC_FILES_CACHEGRAND "*.c" "hashtable/*.c")
list(APPEND SRC_FILES_CACHEGRAND "${CACHEGRAND_CMAKE_CONFIG_C_SRC}")
list(REMOVE_ITEM SRC_FILES_CACHEGRAND ${CMAKE_CURRENT_SOURCE_DIR}/main.c)

# Remove all the architecture dependant implementation but re-include the loop (non architecture dependant)
list(FILTER SRC_FILES_CACHEGRAND EXCLUDE REGEX ".*hashtable_support_hash_search_[a-z0-9]+.c$")
list(APPEND SRC_FILES_CACHEGRAND "${CMAKE_CURRENT_SOURCE_DIR}/hashtable/hashtable_support_hash_search_loop.c")

if(ARCH_IS_X86_64)
    message(STATUS "Enabling accelerated hash search -- avx2")
    list(APPEND SRC_FILES_CACHEGRAND "${CMAKE_CURRENT_SOURCE_DIR}/hashtable/hashtable_support_hash_search_avx2.c")
    set_source_files_properties(
            "hashtable/hashtable_support_hash_search_avx2.c"
            PROPERTIES COMPILE_FLAGS
            "-mno-avx256-split-unaligned-load -mavx2 -mbmi -mfma -mtune=haswell")

    message(STATUS "Enabling accelerated hash search -- avx")
    list(APPEND SRC_FILES_CACHEGRAND "${CMAKE_CURRENT_SOURCE_DIR}/hashtable/hashtable_support_hash_search_avx.c")
    set_source_files_properties(
            "hashtable/hashtable_support_hash_search_avx.c"
            PROPERTIES COMPILE_FLAGS
            "-mno-avx256-split-unaligned-load -mno-avx2 -mavx -mbmi")

    foreach(HASHTABLE_SUPPORT_OP_ARCH_SUFFIX
            avx2
            avx
            sse42
            sse3)
        CACHEGRAND_BUILD_ADHOC_HASHTABLE_SUPPORT_OP_ARCH(
                ${HASHTABLE_SUPPORT_OP_ARCH_SUFFIX})
    endforeach()
else()
    message(FATAL_ERROR "Unsupported architecture")
endif()

add_library(
        cachegrand-internal
        ${SRC_FILES_CACHEGRAND}
)
add_dependencies(cachegrand-internal
        __internal_refresh_cmake_config_c
        t1ha)
target_link_libraries(cachegrand-internal t1ha)
target_include_directories(cachegrand-internal PUBLIC "../3rdparty/t1ha/")
target_include_directories(cachegrand-internal PUBLIC ".")

if(ARCH_IS_X86_64)
    # Force GCC to emit cmpxchg16b inline (supported only on x86_64 platforms)
    target_compile_options(cachegrand-internal PUBLIC -mcx16)
endif()

if(WIN32)
    target_link_libraries(cachegrand-internal imagehlp)
endif(WIN32)

# Build the executable
add_executable(cachegrand "main.c")
add_dependencies(cachegrand cachegrand-internal t1ha)
target_link_libraries(cachegrand cachegrand-internal t1ha)
target_include_directories(cachegrand PUBLIC ".")
