list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

list(APPEND DEPS_LIST_LIBRARIES "pthread")

# Dependencies that are provided by locally installed packages
include("libyaml")
include("openssl")
include("libnuma")
include("libcurl")

# Dependencies built as part of the build process
include("liburing")
include("libcyaml")
include("sentry")
include("t1ha")
include("xxhash")

set(DEPS_LIST_LIBRARIES "${DEPS_LIST_LIBRARIES}" PARENT_SCOPE)
set(DEPS_LIST_INCLUDE_DIRS "${DEPS_LIST_INCLUDE_DIRS}" PARENT_SCOPE)
set(DEPS_LIST_LIBRARY_DIRS "${DEPS_LIST_LIBRARY_DIRS}" PARENT_SCOPE)
set(DEPS_LIST_LIBRARIES_PRIVATE "${DEPS_LIST_LIBRARIES_PRIVATE}" PARENT_SCOPE)
