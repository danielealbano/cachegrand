# Build the base cachegrand library
file(GLOB SRC_FILES_CACHEGRAND "*.c")
file(GLOB SRC_FILES_CACHEGRAND_HASHTABLE "hashtable/*.c")
list(REMOVE_ITEM SRC_FILES_CACHEGRAND ${CMAKE_CURRENT_SOURCE_DIR}/main.c)
add_library(
        cachegrand-internal
        ${SRC_FILES_CACHEGRAND}
        ${SRC_FILES_CACHEGRAND_HASHTABLE}
)
add_dependencies(cachegrand-internal t1ha2)
target_link_libraries(cachegrand-internal t1ha2)
target_include_directories(cachegrand-internal PUBLIC "../3rdparty/t1ha/")
target_include_directories(cachegrand-internal PUBLIC ".")

IF(WIN32)
    target_link_libraries(cachegrand-internal imagehlp)
ENDIF(WIN32)

# Remove all the architecture dependant implementation but reinclude the loop (non architecture dependant)
list(FILTER SRC_FILES_CACHEGRAND_HASHTABLE EXCLUDE REGEX ".*hashtable_support_hash_search_[a-z0-9]+\\.c$")
list(APPEND SRC_FILES_CACHEGRAND_HASHTABLE "${CMAKE_CURRENT_SOURCE_DIR}/hashtable/hashtable_support_hash_search_loop.c")

if(ARCH_IS_X86_64)
    # Check for AVX2 and BMI
    if(COMPILER_HAS_MAVX2_FLAG AND COMPILER_HAS_MBMI_FLAG)
        message(STATUS "Enabling accelerated hash search -- avx2")
        list(APPEND SRC_FILES_CACHEGRAND_HASHTABLE "${CMAKE_CURRENT_SOURCE_DIR}/hashtable/hashtable_support_hash_search_avx2.c")
        set_source_files_properties(
                "hashtable/hashtable_support_hash_search_avx2.c"
                PROPERTIES COMPILE_FLAGS
                "-mno-avx256-split-unaligned-load -mavx2 -mbmi")
    endif(COMPILER_HAS_MAVX2_FLAG AND COMPILER_HAS_MBMI_FLAG)

    # Check for AVX and BMI
    if(COMPILER_HAS_MAVX_FLAG AND COMPILER_HAS_MBMI_FLAG)
        message(STATUS "Enabling accelerated hash search -- avx")
        list(APPEND SRC_FILES_CACHEGRAND_HASHTABLE "${CMAKE_CURRENT_SOURCE_DIR}/hashtable/hashtable_support_hash_search_avx.c")
        set_source_files_properties(
                "hashtable/hashtable_support_hash_search_avx.c"
                PROPERTIES COMPILE_FLAGS
                "-mno-avx256-split-unaligned-load -mavx -mbmi")
    endif(COMPILER_HAS_MAVX_FLAG AND COMPILER_HAS_MBMI_FLAG)
elseif(ARCH_IS_AARCH64)
# TODO
else()
    message(FATAL_ERROR "Unsupported architecture")
endif()

# Build the executable
add_executable(cachegrand "main.c")
add_dependencies(cachegrand cachegrand-internal t1ha2)
target_link_libraries(cachegrand cachegrand-internal t1ha2)
target_include_directories(cachegrand PUBLIC ".")
