file(GLOB SRC_FILES_HWY_C_WRAPPER "*.cc")

add_library(
        hwy_c_wrapper
        SHARED
        ${SRC_FILES_HWY_C_WRAPPER})

set_target_properties(
        hwy_c_wrapper
        PROPERTIES
        SOVERSION 1
        PUBLIC_HEADER vqsort_c_exports.h
        ENABLE_EXPORTS ON
        POSITION_INDEPENDENT_CODE ON)

target_include_directories(
        hwy_c_wrapper
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DEPS_LIST_INCLUDE_DIRS})

target_link_directories(
        hwy_c_wrapper
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DEPS_LIST_LIBRARY_DIRS})

target_link_libraries(
        hwy_c_wrapper
        PRIVATE
        hwy
        hwy_contrib)

add_dependencies(
        hwy_c_wrapper
        hwy-build)

#set(LIBHCY_C_WRAPPER_LIBRARIES hwy)
#set(LIBHCY_C_WRAPPER_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
#list(APPEND DEPS_LIST_LIBRARIES "hwy_c_wrapper")
#list(APPEND DEPS_LIST_INCLUDE_DIRS "${LIBHCY_C_WRAPPER_INCLUDE_DIRS}")
