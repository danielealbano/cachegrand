include(FindPkgConfig)

pkg_check_modules(LIBURING liburing>=0.7)

if (LIBURING_FOUND)
    list(APPEND DEPS_LIST_INCLUDE_DIRS ${LIBURING_INCLUDE_DIRS})
    list(APPEND DEPS_LIST_LIBRARY_DIRS ${LIBURING_LIBRARY_DIRS})
    list(APPEND DEPS_LIST_LIBRARIES ${LIBURING_LIBRARIES})
else()
    set(LIBURING_FOUND "0")

    message(STATUS "liburing 0.7 may haven't been released yet, if you are running debian / ubuntu you can easily " +
            "build it from the sources, all the necessary files to create the proper package are already available")
endif()
