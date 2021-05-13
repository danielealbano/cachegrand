include(FetchContent)

FetchContent_Declare(
        sentry
        GIT_REPOSITORY https://github.com/getsentry/sentry-native
        GIT_TAG        0.4.9
)

FetchContent_MakeAvailable(sentry)

set(SENTRY_LIBRARIES "sentry")
set(SENTRY_INCLUDE_DIRS "${sentry_SOURCE_DIR}/include")
set(SENTRY_LIBRARY_DIRS "${sentry_BINARY_DIR}/src")

list(APPEND DEPS_LIST_LIBRARIES "${SENTRY_LIBRARIES}")
list(APPEND DEPS_LIST_INCLUDE_DIRS "${SENTRY_INCLUDE_DIRS}")
list(APPEND DEPS_LIST_LIBRARY_DIRS "${SENTRY_LIBRARY_DIRS}")
