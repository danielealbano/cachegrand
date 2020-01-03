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
set_target_properties(cachegrand-internal PROPERTIES LINKER_LANGUAGE C)
set_target_properties(cachegrand-internal PROPERTIES C_STANDARD 11)

# Build the exacutable
add_executable(cachegrand "main.c")
add_dependencies(cachegrand cachegrand-internal t1ha2)
target_link_libraries(cachegrand cachegrand-internal t1ha2)
target_include_directories(cachegrand PUBLIC ".")
set_target_properties(cachegrand PROPERTIES LINKER_LANGUAGE C)
set_target_properties(cachegrand PROPERTIES C_STANDARD 11)
