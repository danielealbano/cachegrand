include(CheckCSourceCompiles)

list(APPEND CMAKE_REQUIRED_LIBRARIES "-lmbedtls -lmbedx509 -lmbedcrypto")
check_c_source_compiles("
#include <mbedtls/aes.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/gcm.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_internal.h>

int main() {
    void *fp = mbedtls_ssl_init;
    return 0;
}"
        HOST_HAS_LIBMBEDTLS)
set(CMAKE_REQUIRED_LIBRARIES ${OLD_CMAKE_REQUIRED_LIBRARIES})

if (NOT HOST_HAS_LIBMBEDTLS)
    message(FATAL_ERROR "Can't find libmbedtls")
endif()

find_library(LIBMBEDTLS_LIBRARY_DIRS NAMES mbedtls NAMES_PER_DIR)
find_path(LIBMBEDTLS_INCLUDE_DIRS mbedtls/ssl.h)
set(LIBMBEDTLS_LIBRARIES mbedtls mbedx509 mbedcrypto)

list(APPEND DEPS_LIST_LIBRARIES "${LIBMBEDTLS_LIBRARIES}")
list(APPEND DEPS_LIST_INCLUDE_DIRS "${LIBMBEDTLS_INCLUDE_DIRS}")
list(APPEND DEPS_LIST_LIBRARY_DIRS "${LIBMBEDTLS_LIBRARY_DIRS}")
