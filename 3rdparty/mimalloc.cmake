set(MI_BUILD_OBJECT FALSE CACHE BOOL "mimalloc option overridden" FORCE)
set(MI_BUILD_SHARED FALSE CACHE BOOL "mimalloc option overridden" FORCE)
set(MI_BUILD_TESTS FALSE CACHE BOOL "mimalloc option overridden" FORCE)
set(MI_OVERRIDE FALSE CACHE BOOL "mimalloc option overridden" FORCE)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(MI_SECURE TRUE CACHE BOOL "mimalloc option overridden" FORCE)
    set(MI_DEBUG_FULL TRUE CACHE BOOL "mimalloc option overridden" FORCE)
    set(MI_PADDING TRUE CACHE BOOL "mimalloc option overridden" FORCE)
else()
    set(MI_SECURE FALSE CACHE BOOL "mimalloc option overridden" FORCE)
    set(MI_DEBUG_FULL FALSE CACHE BOOL "mimalloc option overridden" FORCE)
    set(MI_PADDING FALSE CACHE BOOL "mimalloc option overridden" FORCE)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/mimalloc/ EXCLUDE_FROM_ALL)

list(APPEND DEPS_LIST_LIBRARIES "mimalloc-static")
list(APPEND DEPS_LIST_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/mimalloc")
