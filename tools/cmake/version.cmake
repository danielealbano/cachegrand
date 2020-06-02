find_package(Git REQUIRED)
if(NOT GIT_FOUND)
    message(FATAL_ERROR "git not found!")
endif()

# Fetch the git version ([tag|commitid](-dirty))
execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --always --dirty
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE CACHEGRAND_VERSION_GIT
    OUTPUT_STRIP_TRAILING_WHITESPACE)

# Fetch the build date/time
execute_process(COMMAND "date" "+%Y-%m-%d"
        OUTPUT_VARIABLE CACHEGRAND_BUILD_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND "date" --utc +%H:%M:%S
        OUTPUT_VARIABLE CACHEGRAND_BUILD_TIME
        OUTPUT_STRIP_TRAILING_WHITESPACE)

set(CACHEGRAND_BUILD_DATE_TIME "${CACHEGRAND_BUILD_DATE}T${CACHEGRAND_BUILD_TIME}Z")

message(STATUS "Git Version: ${CACHEGRAND_VERSION_GIT}")
message(STATUS "Build date/time: ${CACHEGRAND_BUILD_DATE_TIME}")

message(STATUS "Updating version.c")
configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/version.c.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/version.c"
        @ONLY)
message(STATUS "Updating version.c -- done")
