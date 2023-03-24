list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

list(APPEND DEPS_LIST_LIBRARIES "pthread")

# Dependencies that are provided by locally installed packages
include("libyaml")
include("mbedtls")
include("libnuma")

if (BUILD_TESTS)
    include("libhiredis")
endif()

# Dependencies built as part of the build process
include("libhwy")
include("mimalloc")
include("liburing")
include("libcyaml")
include("sentry")
include("t1ha")
include("xxhash")
include("nodejs-http-parser")

# Dependencies for all the targets
set(DEPS_LIST_LIBRARIES "${DEPS_LIST_LIBRARIES}" PARENT_SCOPE)
set(DEPS_LIST_INCLUDE_DIRS "${DEPS_LIST_INCLUDE_DIRS}" PARENT_SCOPE)
set(DEPS_LIST_LIBRARY_DIRS "${DEPS_LIST_LIBRARY_DIRS}" PARENT_SCOPE)
set(DEPS_LIST_LIBRARIES_PRIVATE "${DEPS_LIST_LIBRARIES_PRIVATE}" PARENT_SCOPE)

# Dependencies for tests only
set(TESTSDEPS_LIST_LIBRARIES "${TESTSDEPS_LIST_LIBRARIES}" PARENT_SCOPE)
set(TESTSDEPS_LIST_INCLUDE_DIRS "${TESTSDEPS_LIST_INCLUDE_DIRS}" PARENT_SCOPE)
set(TESTSDEPS_LIST_LIBRARY_DIRS "${TESTSDEPS_LIST_LIBRARY_DIRS}" PARENT_SCOPE)
