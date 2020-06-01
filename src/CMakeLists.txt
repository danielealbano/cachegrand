# Build the base cachegrand library
file(GLOB SRC_FILES_CACHEGRAND "*.c")
file(GLOB SRC_FILES_CACHEGRAND_HASHTABLE "hashtable/*.c")
LIST(REMOVE_ITEM SRC_FILES_CACHEGRAND ${CMAKE_CURRENT_SOURCE_DIR}/main.c)
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

# For the hashtable hash search algorithm optimized for avx2 enable -mno-avx256-split-unaligned-load -mavx2 -mlzcnt
set_source_files_properties(
        "hashtable/hashtable_support_hash_search_avx2.c"
        PROPERTIES COMPILE_FLAGS
        "-mno-avx256-split-unaligned-load -mavx2 -mlzcnt")

# Build the executable
add_executable(cachegrand "main.c")
add_dependencies(cachegrand cachegrand-internal t1ha2)
target_link_libraries(cachegrand cachegrand-internal t1ha2)
target_include_directories(cachegrand PUBLIC ".")
