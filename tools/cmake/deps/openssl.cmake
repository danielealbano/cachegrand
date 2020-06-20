include(FindOpenSSL)

if (NOT OPENSSL_FOUND)
    message(FATAL "OpenSSL not found, unable to continue")
endif()

message(STATUS "OpenSSL found")
message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")
