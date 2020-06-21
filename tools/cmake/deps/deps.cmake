# Add base libraries always available
list(APPEND DEPS_LIST_LIBRARIES pthread)

include(openssl)
include(liburing)
