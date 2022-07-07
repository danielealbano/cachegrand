include(ExternalProject)
file(GLOB SRC_FILES_NODEJS_HTTP_PARSER "nodejs-http-parser/http_parser.c")

set(NODEJS_HTTP_PARSER_BUILD_PATH "${CMAKE_BINARY_DIR}/_deps/src/nodejs-http-parser-install-build")

add_library(
        nodejs_http_parser
        ${SRC_FILES_NODEJS_HTTP_PARSER})

target_compile_options(
        nodejs_http_parser
        PRIVATE
        -Wall -Wextra -Werror -O2 -fPIC)

target_compile_definitions(
        nodejs_http_parser
        PRIVATE
        HTTP_PARSER_STRICT=0)

target_include_directories(
        nodejs_http_parser
        PUBLIC
        nodejs-http-parser/)

list(APPEND DEPS_LIST_LIBRARIES "nodejs_http_parser")
